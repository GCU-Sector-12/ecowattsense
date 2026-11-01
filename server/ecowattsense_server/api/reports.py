# ecowattsense_server/api/reports.py
"""POST /api/reports (contract 5.4): check each report, store it, and answer one result for each.

A report that is stored is "created". A report the server has already (same device_id and
timestamp) is a "duplicate": it is not stored again and it is not an error. A wrong report is
"rejected". When energy_uj is missing the server stores 0 as "estimated"; the real estimate from
energy_rates is the W7 task "energy_uj filled at write".
"""
from datetime import datetime, timezone
from typing import Annotated, Any, Literal

from fastapi import APIRouter, Body, Depends, Response, status
from fastapi.exceptions import RequestValidationError
from pydantic import ValidationError
from sqlalchemy.exc import IntegrityError
from sqlmodel import Session, select

from ..auth import require_client_key
from ..db import get_session
from ..models import Device, Report
from ..schemas import MAX_REPORTS_PER_REQUEST, ReportIn, ReportResult, ReportResults

router = APIRouter(prefix="/api", tags=["reports"], dependencies=[Depends(require_client_key)])


def utc_now() -> datetime:
    """The server clock in UTC. A function of its own, so the tests can set the time."""
    return datetime.now(timezone.utc)


def validation_error(exc: ValidationError) -> RequestValidationError:
    """The 422 for a wrong single report: the usual FastAPI list, with "body" in front of each place."""
    errors = exc.errors(include_url=False, include_context=False)
    return RequestValidationError([{**error, "loc": ("body", *error["loc"])} for error in errors])


def first_problem(exc: ValidationError) -> str:
    """The first problem in words, for the result of one report in a list."""
    error = exc.errors(include_url=False, include_context=False)[0]
    where = ".".join(str(part) for part in error["loc"])
    return f"{where}: {error['msg']}" if where else error["msg"]


def save(session: Session, report: ReportIn, received_at: datetime) -> Literal["created", "duplicate"]:
    """Store one valid report and move the computer's last_seen_at. A known report is a duplicate."""
    timestamp = report.timestamp.astimezone(timezone.utc)
    device = session.get(Device, report.device_id)
    if device is None:
        # The first report of a computer creates it. No register step and no login.
        device = Device(
            id=report.device_id,
            machine_hash=report.machine_hash,
            os=report.os,
            device_type=report.device_type,
            first_seen_at=received_at,
            last_seen_at=received_at,
        )
    else:
        known = session.exec(
            select(Report.id).where(Report.device_id == report.device_id, Report.timestamp == timestamp)
        ).first()
        if known is not None:
            return "duplicate"
        device.last_seen_at = received_at
        # The hash comes from the first report that carries it. display_name is never touched.
        if device.machine_hash is None and report.machine_hash is not None:
            device.machine_hash = report.machine_hash
    session.add(device)

    # A sent energy_uj is stored as it is, also 0. Without it: 0, "estimated" (see the top of the file).
    measured = report.energy_uj is not None
    session.add(
        Report(
            device_id=report.device_id,
            timestamp=timestamp,
            received_at=received_at,
            interval_seconds=report.interval_seconds,
            state=report.state,
            idle_seconds=report.idle_seconds,
            cpu_percent=report.cpu_percent,
            screen_locked=report.screen_locked,
            on_battery=report.on_battery,
            energy_uj=report.energy_uj if measured else 0,
            energy_source="measured" if measured else "estimated",
            battery_energy_uj=report.battery_energy_uj,
            device_energy_uj=report.device_energy_uj,
        )
    )
    session.commit()
    return "created"


def store_report(session: Session, report: ReportIn, received_at: datetime) -> Literal["created", "duplicate"]:
    """save(), once more when two requests with the same new computer or report arrive together."""
    for attempt in (1, 2):
        try:
            return save(session, report, received_at)
        except IntegrityError:
            # The other request won. Look again: the second time the computer or the report is there.
            session.rollback()
            if attempt == 2:
                raise
    raise AssertionError("unreachable")


@router.post("/reports", response_model=ReportResults, status_code=status.HTTP_201_CREATED)
def post_reports(
    response: Response, session: Annotated[Session, Depends(get_session)], body: Any = Body(...)
) -> ReportResults:
    """One report (201 stored, 200 duplicate), or a list of 1 to 100 reports (200, one result for each)."""
    received_at = utc_now()

    if not isinstance(body, list):
        try:
            report = ReportIn.model_validate(body)
        except ValidationError as exc:
            raise validation_error(exc) from exc
        outcome = store_report(session, report, received_at)
        if outcome == "duplicate":
            response.status_code = status.HTTP_200_OK
        return ReportResults(results=[ReportResult(index=0, status=outcome)])

    if not 1 <= len(body) <= MAX_REPORTS_PER_REQUEST:
        raise RequestValidationError(
            [
                {
                    "type": "too_short" if not body else "too_long",
                    "loc": ("body",),
                    "msg": f"The list must have 1 to {MAX_REPORTS_PER_REQUEST} reports",
                    "input": len(body),
                }
            ]
        )

    # For a list the answer is always 200. A wrong report is "rejected", the others still count.
    response.status_code = status.HTTP_200_OK
    results = []
    for index, item in enumerate(body):
        try:
            report = ReportIn.model_validate(item)
        except ValidationError as exc:
            results.append(ReportResult(index=index, status="rejected", error=first_problem(exc)))
        else:
            results.append(ReportResult(index=index, status=store_report(session, report, received_at)))
    return ReportResults(results=results)
