# ecowattsense_server/models.py
"""Data models for the EcoWattSense server."""
from datetime import datetime
from uuid import UUID

from sqlalchemy import BigInteger, SmallInteger, UniqueConstraint
from sqlmodel import Field, SQLModel


class Report(SQLModel, table=True):
    """One monitored device report."""

    __tablename__ ="devices"

id: UUID =Field(primary_key=True)
display_name: str | None = None
# The locations table comes in W7, so this is a plain number for now, without a foreign key.
location_id: int | None = None
machine_hash: str | None = None
os: str
device_type: str
first_seen_at: datetime
last_seen_at: datetime
archived: bool = False  