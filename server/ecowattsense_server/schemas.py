# ecowattsense_server/schemas.py
"""Shapes of the requests and answers of the API.

ReportIn follows shared/report_schema.json field by field. The rules and the reply
come from docs/design/api-contract.md, section 5.4 (POST /api/reports).
"""
from datetime import datetime, timedelta, timezone
from typing import Annotated, Literal, Optional, Union
from uuid import UUID
import unicodedata

from pydantic import (
    AwareDatetime,
    BaseModel,
    ConfigDict,
    Field,
    StrictBool,
    StrictInt,
    SecretStr,
    model_validator,
    field_validator,
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
ReportList = Annotated[list[ReportIn], Field(min_length=1, max_length=100)]
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


class LoginIn(BaseModel):
    model_config = ConfigDict(extra="forbid", hide_input_in_errors=True)

    username: str
    password: SecretStr


class LoginOut(BaseModel):
    username: str


class SettingsOut(BaseModel):
    settings_version: int
    report_interval_seconds: int
    ping_interval_seconds: int
    idle_threshold_seconds: int
    unused_threshold_seconds: int
    no_data_after_cycles: int
    tariff_valid_from: AwareDatetime
    price_gbp_per_kwh: float
    co2_kg_per_kwh: float


class Ack(BaseModel):
    model_config = ConfigDict(extra="forbid")

    command_id: StrictInt
    result: Literal["done", "failed", "unsupported"]


class PingIn(BaseModel):
    model_config = ConfigDict(extra="forbid")

    device_id: UUID
    unsent_reports: Annotated[StrictInt, Field(ge=0)]
    acks: list[Ack] = Field(default_factory=list)


class CommandOut(BaseModel):
    id: int
    type: str
    params: dict
    expires_at: AwareDatetime


class PingOut(BaseModel):
    settings_version: int
    commands: list[CommandOut]
    registered: bool


class DeviceIn(BaseModel):
    model_config = ConfigDict(extra="forbid")

    device_name: str | None = Field(default=None, max_length=60)
    os: Literal["windows", "linux", "macos"]
    device_type: Annotated[str, Field(pattern=r"^[a-z][a-z0-9_]*$", max_length=40)]

    @field_validator("device_name", mode="before")
    @classmethod
    def clean_name(cls, value):
        if isinstance(value, str):
            if any(unicodedata.category(char) == "Cc" for char in value):
                raise ValueError("device_name must not contain control characters")
            return value.strip() or None
        return value


class DeviceOut(BaseModel):
    device_id: UUID
    device_name: str | None
    created: bool


class DashboardDevice(BaseModel):
    device_id: UUID
    display_name: str | None
    location_id: int | None
    location_name: str | None
    building: str | None
    os: Literal["windows", "linux", "macos"]
    device_type: str
    state: Literal[2, 1, 0] | None
    state_name: Literal["active", "idle", "unused", "no_data"]
    last_seen_at: AwareDatetime
    first_seen_at: AwareDatetime
    archived: bool


class DevicesOut(BaseModel):
    devices: list[DashboardDevice]


class StateCounts(BaseModel):
    active: int
    idle: int
    unused: int
    no_data: int
    total: int


class EnergyPoint(BaseModel):
    timestamp: AwareDatetime
    energy_uj: int


class SummaryOut(BaseModel):
    date: str
    counts: StateCounts
    energy_uj: int
    cost_gbp: float
    co2_kg: float
    price_gbp_per_kwh: float
    co2_kg_per_kwh: float
    series: list[EnergyPoint]


class HistoryReport(BaseModel):
    timestamp: AwareDatetime
    interval_seconds: int
    state: Literal[2, 1, 0]
    idle_seconds: int
    cpu_percent: float
    screen_locked: bool
    on_battery: bool
    energy_uj: int
    energy_source: Literal["measured", "estimated"]


class HistoryTotals(BaseModel):
    active_time_seconds: int = 0
    idle_time_seconds: int = 0
    unused_time_seconds: int = 0
    energy_uj: int = 0
    cost_gbp: float = 0
    co2_kg: float = 0


class HistoryOut(BaseModel):
    model_config = ConfigDict(populate_by_name=True)

    device_id: UUID
    display_name: str | None
    from_time: AwareDatetime = Field(alias="from")
    to_time: AwareDatetime = Field(alias="to")
    reports: list[HistoryReport]
    totals: HistoryTotals


class Tariff(BaseModel):
    valid_from: AwareDatetime
    price_gbp_per_kwh: float
    co2_kg_per_kwh: float
