# ecowattsense_server/schemas.py
"""Shapes of the requests and answers of the API.

ReportIn follows shared/report_schema.json field by field. The rules and the reply
come from docs/design/api-contract.md, section 5.4 (POST /api/reports).
"""
from datetime import datetime, timedelta, timezone
from typing import Annotated, Literal, Optional, Union
from uuid import UUID

from pydantic import (
    AwareDatetime,
    BaseModel,
    ConfigDict,
    Field,
    StrictBool,
    StrictInt,
    model_validator,
)

# A report may be at most this far ahead of the server clock (contract 5.4).
MAX_CLOCK_AHEAD = timedelta(minutes=5)

# Fields the client may leave out. They may be missing, but never null.
OPTIONAL_FIELDS = ("energy_uj", "battery_energy_uj", "device_energy_uj", "machine_hash")


class ReportIn(BaseModel):
    """One report from one computer for one interval."""

    # A field that is not in the schema is rejected (422).
    model_config = ConfigDict(extra="forbid")

    device_id: UUID
    # AwareDatetime: a time without a zone is rejected.
    timestamp: AwareDatetime
    interval_seconds: Annotated[StrictInt, Field(gt=0)]
    # 2 active, 1 idle, 0 unused.
    state: Literal[2, 1, 0]
    idle_seconds: Annotated[StrictInt, Field(ge=0)]
    cpu_percent: Annotated[float, Field(ge=0, le=100)]
    screen_locked: StrictBool
    on_battery: StrictBool
    os: Literal["windows", "linux", "macos"]
    device_type: Annotated[str, Field(pattern=r"^[a-z][a-z0-9_]*$", max_length=40)]

    energy_uj: Optional[Annotated[StrictInt, Field(ge=0)]] = None
    # Negative = energy out of the battery, positive = into it.
    battery_energy_uj: Optional[StrictInt] = None
    device_energy_uj: Optional[Annotated[StrictInt, Field(ge=0)]] = None
    machine_hash: Optional[Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]] = None

    @model_validator(mode="before")
    @classmethod
    def optional_fields_are_not_null(cls, data):
        # The schema types these fields as integer or string, so null is not allowed.
        if isinstance(data, dict):
            for name in OPTIONAL_FIELDS:
                if name in data and data[name] is None:
                    raise ValueError(f"{name} can be missing, but it cannot be null")
        return data

    @model_validator(mode="after")
    def timestamp_not_too_far_ahead(self):
        if self.timestamp > datetime.now(timezone.utc) + MAX_CLOCK_AHEAD:
            raise ValueError("timestamp is more than 5 minutes ahead of the server clock")
        return self

    @model_validator(mode="after")
    def no_wall_energy_on_battery(self):
        # A computer on its battery takes nothing from the wall socket.
        if self.on_battery and self.energy_uj is not None and self.energy_uj > 0:
            raise ValueError("energy_uj must be 0 when on_battery is true")
        return self


# The body of POST /api/reports: one report, or a list of 1 to 100 reports.
MAX_REPORTS_PER_REQUEST = 100
ReportList = Annotated[list[ReportIn], Field(min_length=1, max_length=MAX_REPORTS_PER_REQUEST)]
ReportBody = Union[ReportIn, ReportList]


class ReportResult(BaseModel):
    """The answer for one report."""

    index: int
    status: Literal["created", "duplicate", "rejected"]
    # The first problem, in words. A client must not parse it.
    error: Optional[str] = None


class ReportResults(BaseModel):
    """The reply of POST /api/reports: one result for each report, in the same order."""

    results: list[ReportResult]
