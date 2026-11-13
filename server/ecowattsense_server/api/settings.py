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
    return {row.key: row.value for row in session.exec(select(Setting)).all()}


def integer_setting(values: dict[str, str], key: str) -> int:
    return int(values.get(key, getattr(settings, key)))


def current_settings_version(session: Session) -> int:
    query = select(Setting.value).where(Setting.key == SETTINGS_VERSION_KEY)
    value = session.exec(query).first()
    return FIRST_SETTINGS_VERSION if value is None else int(value)


def tariff_in_force(session: Session, now: datetime) -> Tariff | None:
    query = (select(Tariff)
             .where(Tariff.valid_from <= now)
             .order_by(Tariff.valid_from.desc()))
    return session.exec(query).first()


def as_utc(moment: datetime) -> datetime:
    return moment if moment.tzinfo else moment.replace(tzinfo=timezone.utc)


@router.get("/settings", response_model=SettingsOut)
def get_settings(session: Annotated[Session, Depends(get_session)]) -> SettingsOut:
    values = stored_values(session)
    tariff = tariff_in_force(session, datetime.now(timezone.utc))
    return SettingsOut(
        settings_version=current_settings_version(session),
        **{key: integer_setting(values, key) for key in INTEGER_KEYS},
        tariff_valid_from=as_utc(tariff.valid_from),
        price_gbp_per_kwh=tariff.price_gbp_per_kwh,
        co2_kg_per_kwh=tariff.co2_kg_per_kwh,
    )