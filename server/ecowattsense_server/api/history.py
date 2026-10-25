"""Device history with exclusive end time and report-time tariff totals."""
from datetime import datetime, timedelta, timezone
from typing import Annotated
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, Query
from fastapi.exceptions import RequestValidationError
from pydantic import AwareDatetime
from sqlmodel import Session

from ..auth import require_admin
from ..config import settings
from ..dashboard_store import history_from_database
from ..dashboard_totals import report_totals
from ..db import get_session
from ..demo_data import demo_devices, demo_history, demo_tariffs
from ..schemas import HistoryOut

router = APIRouter(prefix="/api", tags=["history"], dependencies=[Depends(require_admin)])


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


@router.get("/devices/{device_id}/history", response_model=HistoryOut)
def history(
    device_id: UUID,
    session: Annotated[Session, Depends(get_session)],
    from_time: Annotated[AwareDatetime | None, Query(alias="from")] = None,
    to_time: Annotated[AwareDatetime | None, Query(alias="to")] = None,
) -> HistoryOut:
    end = (to_time or utc_now()).astimezone(timezone.utc)
    try:
        start = from_time.astimezone(timezone.utc) if from_time else end - timedelta(hours=24)
    except OverflowError as exc:
        raise RequestValidationError([{"type": "value_error", "loc": ("query", "to"),
            "msg": "to is outside the supported date range", "input": str(end)}]) from exc
    if start >= end or end - start > timedelta(days=31):
        raise RequestValidationError([{"type": "value_error", "loc": ("query", "from"),
            "msg": "Range must be positive and at most 31 days", "input": str(start)}])
    if settings.dashboard_data_source == "database":
        return history_from_database(session, device_id, start, end)
    device = next((row for row in demo_devices() if row.device_id == device_id), None)
    if device is None:
        raise HTTPException(404, "Device not found")
    reports = sorted((report for report in demo_history(device_id)
                      if start <= report.timestamp < end), key=lambda report: report.timestamp)
    return HistoryOut(device_id=device_id, display_name=device.display_name,
                      from_time=start, to_time=end, reports=reports,
                      totals=report_totals(reports, demo_tariffs()))
