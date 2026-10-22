# ecowattsense_server/api/reports.py
"""POST /api/reports (contract 5.4). W6 stub: a valid report gets "created" and nothing is stored.

Storage, duplicates and the energy rules come with EWS_API-13.
"""
from typing import Any

from fastapi import APIRouter, Body, Response, status
from fastapi.exceptions import RequestValidationError
from pydantic import ValidationError

from ..schemas import MAX_REPORTS_PER_REQUEST, ReportIn, ReportResult, ReportResults

# EWS_API-7: add dependencies=[Depends(...)] here for the X-EWS-Key check (401) when auth.py is in.
router = APIRouter(prefix="/api", tags=["reports"])


def validation_error(exc: ValidationError) -> RequestValidationError:
    """The 422 for a wrong single report: the usual FastAPI list, with "body" in front of each place."""
    errors = exc.errors(include_url=False, include_context=False)
    return RequestValidationError([{**error, "loc": ("body", *error["loc"])} for error in errors])


def first_problem(exc: ValidationError) -> str:
    """The first problem in words, for the result of one report in a list."""
    error = exc.errors(include_url=False, include_context=False)[0]
    where = ".".join(str(part) for part in error["loc"])
    return f"{where}: {error['msg']}" if where else error["msg"]


@router.post("/reports", response_model=ReportResults, status_code=status.HTTP_201_CREATED)
def post_reports(response: Response, body: Any = Body(...)) -> ReportResults:
    """One report (201), or a list of 1 to 100 reports (200, one result for each report)."""
    if not isinstance(body, list):
        try:
            ReportIn.model_validate(body)
        except ValidationError as exc:
            raise validation_error(exc) from exc
        return ReportResults(results=[ReportResult(index=0, status="created")])

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
            ReportIn.model_validate(item)
        except ValidationError as exc:
            results.append(ReportResult(index=index, status="rejected", error=first_problem(exc)))
        else:
            results.append(ReportResult(index=index, status="created"))
    return ReportResults(results=results)
