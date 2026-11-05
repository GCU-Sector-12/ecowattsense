# ecowattsense_server/models.py
"""Database tables. The columns follow docs/design/database-data-brief.md, sections 5.2 and 5.3."""
from datetime import datetime
from uuid import UUID

from sqlalchemy import BigInteger, CheckConstraint, SmallInteger, UniqueConstraint
from sqlmodel import Field, SQLModel


class Device(SQLModel, table=True):
    """One monitored computer."""

    __tablename__ = "devices"

    id: UUID = Field(primary_key=True)
    display_name: str | None = None
    # The locations table comes in W7, so this is a plain number for now, without a foreign key.
    location_id: int | None = None
    machine_hash: str | None = None
    os: str
    device_type: str
    first_seen_at: datetime
    last_seen_at: datetime
    archived: bool = False


class Report(SQLModel, table=True):
    """One report from one computer for one interval. All times are UTC."""

    __tablename__ = "reports"
    # The same device and timestamp together can only be stored once.
    __table_args__ = (UniqueConstraint("device_id", "timestamp"),)

    id: int | None = Field(default=None, primary_key=True)
    device_id: UUID = Field(foreign_key="devices.id")
    timestamp: datetime = Field(index=True)
    received_at: datetime
    interval_seconds: int
    state: int = Field(sa_type=SmallInteger)
    idle_seconds: int
    cpu_percent: float
    screen_locked: bool
    on_battery: bool
    # Energy is in microjoules. One report can be bigger than 32 bits, so it is a 64-bit integer.
    energy_uj: int = Field(sa_type=BigInteger)
    energy_source: str
    battery_energy_uj: int | None = Field(default=None, sa_type=BigInteger)
    device_energy_uj: int | None = Field(default=None, sa_type=BigInteger)

class Location(SQLModel, table=True):
    """A room that holds computers. A computer can be in no room."""

    __tablename__ = "locations"

    id: int | None = Field(default=None, primary_key=True)
    name: str = Field(unique=True)
    building: str | None = None


class EnergyRate(SQLModel, table=True):
    """Estimated watts for one device type, for a computer that is plugged in."""

    __tablename__ = "energy_rates"
    # The watts are 0 or more (brief 5.4).
    __table_args__ = (
        CheckConstraint("active_watts >= 0", name="ck_energy_rates_active_watts"),
        CheckConstraint("idle_watts >= 0", name="ck_energy_rates_idle_watts"),
        CheckConstraint("unused_watts >= 0", name="ck_energy_rates_unused_watts"),
    )

    id: int | None = Field(default=None, primary_key=True)
    device_type: str = Field(unique=True)
    active_watts: float
    idle_watts: float
    unused_watts: float


class Tariff(SQLModel, table=True):
    """The price and the CO2 of 1 kWh from a moment on. A row is valid until the next row starts."""

    __tablename__ = "tariffs"
    # The price and the CO2 are 0 or more (brief 5.5).
    __table_args__ = (
        CheckConstraint("price_gbp_per_kwh >= 0", name="ck_tariffs_price_gbp_per_kwh"),
        CheckConstraint("co2_kg_per_kwh >= 0", name="ck_tariffs_co2_kg_per_kwh"),
    )

    id: int | None = Field(default=None, primary_key=True)
    valid_from: datetime = Field(unique=True)
    price_gbp_per_kwh: float
    co2_kg_per_kwh: float


class Setting(SQLModel, table=True):
    """One setting of the system, as text. Example: key report_interval_seconds, value 300."""

    __tablename__ = "settings"

    id: int | None = Field(default=None, primary_key=True)
    key: str = Field(unique=True)
    value: str
