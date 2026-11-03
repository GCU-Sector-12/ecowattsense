# ecowattsense_server/api/reports.py
"""POST /api/reports (contract 5.4): check each report, store it, and answer one result for each.

The clients call this endpoint with the header X-EWS-Key. The body is one report or a list of
1 to 100 reports. A stored report is "created". A report the server has already (same device_id
and timestamp) is a "duplicate": it is not stored again and it is not an error. A wrong report
is "rejected" (in a list) or a 422 (alone).

When energy_uj is missing the server stores 0 as "estimated". The real estimate from
energy_rates is the W7 task "energy_uj filled at write".

Every function here has at most one block (if, for, try, with), see KB EWS-A-25.
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

Outcome = Literal["created", "duplicate"]


def utc_now() -> datetime:
    """Give the server clock in UTC.

    It is a function of its own, so the tests can set the time.

    Returns:
        The current time with the UTC time zone.
    """
    return datetime.now(timezone.utc)


def validation_error(exc: ValidationError) -> RequestValidationError:
    """Turn the problems of one report into the usual FastAPI 422 error.

    "body" is put in front of each place, so the client sees where the problem is.

    Args:
        exc: The error from ReportIn.

    Returns:
        An error that FastAPI answers with the code 422 and a list of the problems.
    """
    errors = exc.errors(include_url=False, include_context=False)
    return RequestValidationError([{**error, "loc": ("body", *error["loc"])} for error in errors])


def first_problem(exc: ValidationError) -> str:
    """Write the first problem of one report in words.

    Used for the result of a wrong report inside a list.

    Args:
        exc: The error from ReportIn.

    Returns:
        The place and the message, for example "cpu_percent: Input should be less than or equal to 100".
    """
    error = exc.errors(include_url=False, include_context=False)[0]
    where = ".".join(str(part) for part in error["loc"])
    return f"{where}: {error['msg']}" if where else error["msg"]


def is_known(session: Session, device_id: Any, timestamp: datetime) -> bool:
    """Tell if the server already stores this report.

    A report is known when the same computer and the same timestamp are stored.

    Args:
        session: The database session.
        device_id: The id of the computer.
        timestamp: The end of the interval of the report, in UTC.

    Returns:
        True when the report is stored, False when it is new.
    """
    query = select(Report.id).where(Report.device_id == device_id, Report.timestamp == timestamp)
    return session.exec(query).first() is not None


def new_device(report: ReportIn, received_at: datetime) -> Device:
    """Make the row of a computer that sends its first report.

    There is no register step and no login: the first report creates the computer.
    display_name and the location stay empty, the admin sets them later.

    Args:
        report: The first report of the computer.
        received_at: The server time of the request. It is first_seen_at and last_seen_at.

    Returns:
        A Device that is not saved yet.
    """
    return Device(
        id=report.device_id,
        machine_hash=report.machine_hash,
        os=report.os,
        device_type=report.device_type,
        first_seen_at=received_at,
        last_seen_at=received_at,
    )


def touch_device(device: Device, report: ReportIn, received_at: datetime) -> None:
    """Update a known computer for a new report.

    last_seen_at moves to the server time. The hash comes from the first report that carries it.
    display_name, os and device_type are never changed by a report.

    Args:
        device: The stored computer.
        report: The new report of the computer.
        received_at: The server time of the request.
    """
    device.last_seen_at = received_at
    if device.machine_hash is None:
        device.machine_hash = report.machine_hash


def device_for(session: Session, report: ReportIn, received_at: datetime) -> Device:
    """Give the computer of a report: the stored one, updated, or a new one.

    Args:
        session: The database session.
        report: The report.
        received_at: The server time of the request.

    Returns:
        The Device to save together with the report.
    """
    device = session.get(Device, report.device_id)
    if device is None:
        return new_device(report, received_at)
    touch_device(device, report, received_at)
    return device


def new_report(report: ReportIn, timestamp: datetime, received_at: datetime) -> Report:
    """Make the database row of a report.

    A sent energy_uj is stored as it is, also 0, and the source is "measured". Without it the
    server stores 0 and the source is "estimated" (see the top of the file).

    Args:
        report: The checked report.
        timestamp: The end of the interval, in UTC.
        received_at: The server time of the request.

    Returns:
        A Report that is not saved yet.
    """
    measured = report.energy_uj is not None
    return Report(
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


def save(session: Session, report: ReportIn, received_at: datetime) -> Outcome:
    """Store one checked report, and create or update its computer.

    A report the server already has is a duplicate: nothing is changed, not even last_seen_at,
    because only an accepted report moves it.

    Args:
        session: The database session.
        report: The checked report.
        received_at: The server time of the request.

    Returns:
        "created" when the report is stored, "duplicate" when the server had it already.

    Raises:
        IntegrityError: When two requests with the same new computer or the same report are
            saved at the same moment. store_report() handles it.
    """
    timestamp = report.timestamp.astimezone(timezone.utc)
    if is_known(session, report.device_id, timestamp):
        return "duplicate"
    session.add(device_for(session, report, received_at))
    session.add(new_report(report, timestamp, received_at))
    session.commit()
    return "created"


def store_report(session: Session, report: ReportIn, received_at: datetime) -> Outcome:
    """Store one checked report, also when two requests arrive together.

    If the database refuses the first try (the other request won), we look again. The second
    time the computer or the report is already stored, and the answer is "duplicate".

    Args:
        session: The database session.
        report: The checked report.
        received_at: The server time of the request.

    Returns:
        "created" or "duplicate".

    Raises:
        IntegrityError: When the second try fails too.
    """
    try:
        return save(session, report, received_at)
    except IntegrityError:
        session.rollback()
    return save(session, report, received_at)


def validate_report(body: Any) -> ReportIn:
    """Check the body of a request that holds one report.

    Args:
        body: The JSON of the request.

    Returns:
        The checked report.

    Raises:
        RequestValidationError: When the report is wrong. FastAPI answers 422.
    """
    try:
        return ReportIn.model_validate(body)
    except ValidationError as exc:
        raise validation_error(exc) from exc


def store_single(session: Session, body: Any, response: Response, received_at: datetime) -> ReportResults:
    """Handle a request with one report: 201 when stored, 200 when it is a duplicate.

    Args:
        session: The database session.
        body: The JSON of the request.
        response: The answer, to set the code 200 for a duplicate.
        received_at: The server time of the request.

    Returns:
        The reply with one result, index 0.

    Raises:
        RequestValidationError: When the report is wrong. FastAPI answers 422.
    """
    outcome = store_report(session, validate_report(body), received_at)
    if outcome == "duplicate":
        response.status_code = status.HTTP_200_OK
    return ReportResults(results=[ReportResult(index=0, status=outcome)])


def check_list_size(body: list) -> None:
    """Check that a list holds 1 to 100 reports.

    Args:
        body: The list from the request.

    Raises:
        RequestValidationError: When the list is empty or too long. FastAPI answers 422.
    """
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


def store_item(session: Session, index: int, item: Any, received_at: datetime) -> ReportResult:
    """Handle one report of a list.

    A wrong report is "rejected" with the first problem in words. It does not stop the others.

    Args:
        session: The database session.
        index: The place of the report in the list.
        item: The JSON of the report.
        received_at: The server time of the request.

    Returns:
        The result of this report: "created", "duplicate" or "rejected".
    """
    try:
        report = ReportIn.model_validate(item)
    except ValidationError as exc:
        return ReportResult(index=index, status="rejected", error=first_problem(exc))
    return ReportResult(index=index, status=store_report(session, report, received_at))


def store_list(session: Session, body: list, received_at: datetime) -> ReportResults:
    """Handle a request with a list of reports, one by one in the order of the request.

    Args:
        session: The database session.
        body: The list from the request.
        received_at: The server time of the request.

    Returns:
        The reply with one result for each report.

    Raises:
        RequestValidationError: When the list is empty or too long. FastAPI answers 422.
    """
    check_list_size(body)
    return ReportResults(results=[store_item(session, index, item, received_at) for index, item in enumerate(body)])


@router.post("/reports", response_model=ReportResults, status_code=status.HTTP_201_CREATED)
def post_reports(
    response: Response, session: Annotated[Session, Depends(get_session)], body: Any = Body(...)
) -> ReportResults:
    """Receive one report or a list of reports from a client and store them.

    The key is checked first (401). One report gets 201 when stored and 200 when it is a
    duplicate. A list always gets 200, with one result for each report.

    Args:
        response: The answer, to set the status code.
        session: The database session.
        body: The JSON of the request: one report, or a list of 1 to 100 reports.

    Returns:
        The reply: a list of results.

    Raises:
        RequestValidationError: For a wrong single report, or a list of 0 or more than 100
            reports. FastAPI answers 422.
    """
    received_at = utc_now()
    if isinstance(body, list):
        response.status_code = status.HTTP_200_OK
        return store_list(session, body, received_at)
    return store_single(session, body, response, received_at)
