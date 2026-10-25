"""GET /api/settings: W6 values only; persistence comes in W7."""
from datetime import datetime, timezone

from fastapi import APIRouter, Depends

from ..auth import require_client_key
from ..config import settings
from ..schemas import SettingsOut

router = APIRouter(prefix="/api", tags=["settings"], dependencies=[Depends(require_client_key)])


@router.get("/settings", response_model=SettingsOut)
def get_settings() -> SettingsOut:
    return SettingsOut(
        settings_version=1,
        report_interval_seconds=settings.report_interval_seconds,
        ping_interval_seconds=settings.ping_interval_seconds,
        idle_threshold_seconds=settings.idle_threshold_seconds,
        unused_threshold_seconds=settings.unused_threshold_seconds,
        no_data_after_cycles=settings.no_data_after_cycles,
        tariff_valid_from=datetime(2000, 1, 1, tzinfo=timezone.utc),
        price_gbp_per_kwh=0.25,
        co2_kg_per_kwh=0.207,
    )
