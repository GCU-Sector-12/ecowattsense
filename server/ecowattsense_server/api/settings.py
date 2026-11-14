"""GET /api/settings (contract 5.2): the settings and the tariff in force, read from the database.

The clients call this endpoint with the header X-EWS-Key. They call it when they start and when
the answer to POST /api/ping has a settings_version they do not have.

The values come from the settings table (brief 5.6) and the tariffs table (brief 5.5).
settings_version is the row with the key settings_version in the settings table. The admin code
raises it by 1 when it changes a setting or the tariff. A database without that row is at
version 1. A setting with no row uses the value from the server configuration.
"""
from datetime import datetime, timezone
from typing import Annotated

from fastapi import APIRouter, Depends
from sqlmodel import Session, select

from ..auth import require_client_key
from ..config import settings
from ..db import get_session
from ..models import Setting, Tariff
from ..schemas import SettingsOut

router = APIRouter(prefix="/api", tags=["settings"], dependencies=[Depends(require_client_key)])

SETTINGS_VERSION_KEY = "settings_version"
FIRST_SETTINGS_VERSION = 1

# The settings the client gets. Each one is stored as text and sent as a whole number.
INTEGER_KEYS = (
    "report_interval_seconds",
    "ping_interval_seconds",
    "idle_threshold_seconds",
    "unused_threshold_seconds",
    "no_data_after_cycles",
)


def stored_values(session: Session) -> dict[str, str]:
    """Read every row of the settings table.

    Args:
        session: The database session.

    Returns:
        The value of each key, as text.
    """
    return {row.key: row.value for row in session.exec(select(Setting)).all()}


def integer_setting(values: dict[str, str], key: str) -> int:
    """Give one setting as a whole number.

    Args:
        values: The rows of the settings table, key to value.
        key: The name of the setting.

    Returns:
        The stored value, or the value from the server configuration when the key has no row.

    Raises:
        ValueError: The stored text is not a whole number.
    """
    return int(values.get(key, getattr(settings, key)))


def current_settings_version(session: Session) -> int:
    """Give the settings_version the clients must have.

    POST /api/ping sends the same number, so the client knows when to pull the settings again.

    Args:
        session: The database session.

    Returns:
        The stored version, or 1 when the settings table has no settings_version row.

    Raises:
        ValueError: The stored text is not a whole number.
    """
    query = select(Setting.value).where(Setting.key == SETTINGS_VERSION_KEY)
    value = session.exec(query).first()
    return FIRST_SETTINGS_VERSION if value is None else int(value)


def tariff_in_force(session: Session, now: datetime) -> Tariff | None:
    """Find the tariff that is valid now: the newest row that has started.

    Args:
        session: The database session.
        now: The current time in UTC.

    Returns:
        The tariff in force, or None when no tariff has started. The seed adds one from
        2000-01-01, so a normal database always has one.
    """
    query = (select(Tariff)
             .where(Tariff.valid_from <= now)
             .order_by(Tariff.valid_from.desc()))
    return session.exec(query).first()


def as_utc(moment: datetime) -> datetime:
    """Mark a time from the database as UTC.

    Args:
        moment: A time from SQLite, which comes back without a zone.

    Returns:
        The same time with the UTC zone.
    """
    return moment if moment.tzinfo else moment.replace(tzinfo=timezone.utc)


@router.get("/settings", response_model=SettingsOut)
def get_settings(session: Annotated[Session, Depends(get_session)]) -> SettingsOut:
    """Answer the settings and the tariff in force (contract 5.2).

    Args:
        session: The database session.

    Returns:
        The settings_version, the five settings and the tariff in force.

    Raises:
        ValueError: A stored setting is not a whole number (answered with 500).
        AttributeError: The tariffs table has no tariff that has started (answered with 500).
    """
    values = stored_values(session)
    tariff = tariff_in_force(session, datetime.now(timezone.utc))
    return SettingsOut(
        settings_version=current_settings_version(session),
        **{key: integer_setting(values, key) for key in INTEGER_KEYS},
        tariff_valid_from=as_utc(tariff.valid_from),
        price_gbp_per_kwh=tariff.price_gbp_per_kwh,
        co2_kg_per_kwh=tariff.co2_kg_per_kwh,
    )
