# ecowattsense_server/models.py
"""Database tables. The columns follow docs/design/database-data-brief.md, sections 5.2 and 5.3."""
from datetime import datetime
from uuid import UUID

from sqlalchemy import BigInteger, SmallInteger, UniqueConstraint
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
