"""Admin settings reads and atomic saves used by the dashboard Settings page."""
from datetime import timezone
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException
from pydantic import AwareDatetime, BaseModel, ConfigDict, Field, StrictFloat, StrictInt, model_validator
from sqlalchemy import update
from sqlalchemy.dialects.sqlite import insert
from sqlmodel import Session

from ..auth import require_admin
from ..db import get_session
from ..models import Setting, Tariff
from ..schemas import SettingsOut
from .request_validation import require_json
from .settings import INTEGER_KEYS, SETTINGS_VERSION_KEY, get_settings

router = APIRouter(prefix="/api/admin", tags=["admin settings"], dependencies=[Depends(require_admin)])
PositiveInteger = Annotated[StrictInt, Field(gt=0, le=2147483647)]
NonnegativeNumber = Annotated[StrictFloat, Field(ge=0, allow_inf_nan=False)]


class AdminSettingsIn(BaseModel):
    """Complete settings save, including the version read by the admin."""

    model_config = ConfigDict(extra="forbid")
    settings_version: PositiveInteger
    report_interval_seconds: Annotated[StrictInt, Field(ge=60, le=3600)]
    ping_interval_seconds: PositiveInteger
    idle_threshold_seconds: PositiveInteger
    unused_threshold_seconds: PositiveInteger
    no_data_after_cycles: PositiveInteger
    tariff_valid_from: AwareDatetime
    price_gbp_per_kwh: NonnegativeNumber
    co2_kg_per_kwh: NonnegativeNumber

    @model_validator(mode="after")
    def ordered_thresholds(self):
        """Validate the order of the two inactivity thresholds.

        Args:
            self: Validated setting fields.
        Returns:
            This request with its values unchanged.
        Raises:
            ValueError: Unused starts at or before idle.
        """
        if self.unused_threshold_seconds <= self.idle_threshold_seconds:
            raise ValueError("unused_threshold_seconds must be greater than idle_threshold_seconds")
        return self


@router.get("/settings", response_model=SettingsOut)
def read_settings(session: Annotated[Session, Depends(get_session)]) -> SettingsOut:
    """Read the settings and current tariff through admin authentication.

    Args:
        session: Database session supplied by FastAPI.
    Returns:
        The same values as the client settings endpoint.
    Raises:
        ValueError: A stored setting is invalid.
    """
    return get_settings(session)


def claim_version(session: Session, expected: int) -> int:
    """Reserve the next settings version in the current transaction.

    Args:
        session: Database session used for the entire save.
        expected: Version the admin read before editing.
    Returns:
        The next settings version.
    Raises:
        HTTPException: Another save has already changed the version.
    """
    # This write takes SQLite's write lock before any version read, including old databases.
    session.execute(insert(Setting).values(key=SETTINGS_VERSION_KEY, value="1")
                    .on_conflict_do_nothing(index_elements=["key"]))
    statement = update(Setting).where(Setting.key == SETTINGS_VERSION_KEY,
                                      Setting.value == str(expected)).values(value=str(expected + 1))
    result = session.execute(statement)
    if result.rowcount != 1:
        session.rollback()
        raise HTTPException(409, "Settings changed. Reload the settings before saving again.")
    return expected + 1


def write_setting(session: Session, body: AdminSettingsIn, key: str) -> None:
    """Store one integer setting without requiring a pre-existing seed row.

    Args:
        session: Transaction that holds the settings version lock.
        body: Validated request.
        key: Integer setting name.
    Returns:
        None.
    Raises:
        SQLAlchemyError: The database rejects the write.
    """
    value = str(getattr(body, key))
    statement = insert(Setting).values(key=key, value=value)
    session.execute(statement.on_conflict_do_update(index_elements=["key"], set_={"value": value}))


def write_tariff(session: Session, body: AdminSettingsIn) -> None:
    """Store the tariff at its normalized UTC instant, preserving other periods.

    Args:
        session: Transaction shared with the settings updates.
        body: Validated price, CO2 and effective time.
    Returns:
        None.
    Raises:
        SQLAlchemyError: The tariff cannot be written.
    """
    values = {"price_gbp_per_kwh": body.price_gbp_per_kwh, "co2_kg_per_kwh": body.co2_kg_per_kwh}
    moment = body.tariff_valid_from.astimezone(timezone.utc)
    statement = insert(Tariff).values(valid_from=moment, **values)
    session.execute(statement.on_conflict_do_update(index_elements=["valid_from"], set_=values))


@router.put("/settings", response_model=SettingsOut, dependencies=[Depends(require_json)])
def save_settings(body: AdminSettingsIn, session: Annotated[Session, Depends(get_session)]) -> SettingsOut:
    """Save all settings and the submitted tariff as one versioned transaction.

    Args:
        body: Complete validated settings form and its original version.
        session: Database session supplied by FastAPI.
    Returns:
        Saved fields and incremented version; the tariff may start in the future.
    Raises:
        HTTPException: The submitted version is stale.
        SQLAlchemyError: A database write fails; closing the session rolls it back.
    """
    version = claim_version(session, body.settings_version)
    for key in INTEGER_KEYS:
        write_setting(session, body, key)
    write_tariff(session, body)
    response = SettingsOut(**body.model_dump(exclude={"settings_version", "tariff_valid_from"}),
                           settings_version=version, tariff_valid_from=body.tariff_valid_from.astimezone(timezone.utc))
    session.commit()
    return response
