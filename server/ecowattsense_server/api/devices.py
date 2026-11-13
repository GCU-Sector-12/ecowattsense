"""The devices endpoints: PUT /api/devices/{device_id} for the clients, GET /api/devices for the dashboard.

PUT (contract 5.3) registers a computer or changes its name, os and device_type. The clients call
it with the header X-EWS-Key when they first connect and when the user changes the name. The
call can be repeated: created is true only for the call that adds the computer.
GET lists the computers for the admin dashboard.
"""
from uuid import UUID
from datetime import datetime, timezone
from typing import Annotated
from sqlalchemy.exc import IntegrityError
from sqlmodel import Session

from ..config import settings
from ..db import get_session
from ..dashboard_store import devices_from_database

from fastapi import APIRouter, Depends

from ..auth import require_admin, require_client_key
from ..demo_data import demo_devices
from ..models import Device
from ..schemas import DeviceIn, DeviceOut, DevicesOut
from .request_validation import require_json

router = APIRouter(prefix="/api", tags=["devices"])


def utc_now() -> datetime:
    """Give the server clock in UTC.

    It is a function of its own, so the tests can set the time.

    Returns:
        The current time with the UTC time zone.
    """
    return datetime.now(timezone.utc)


def new_device(device_id: UUID, body: DeviceIn, now: datetime) -> Device:
    """Make the row of a computer that registers before its first report.

    Args:
        device_id: The id of the computer, from the path.
        body: The registration: device_name, os and device_type.
        now: The time of the registration. It is first_seen_at and last_seen_at (contract 5.3).

    Returns:
        A Device that is not saved yet.
    """
    return Device(
        id=device_id,
        display_name=body.device_name,
        os=body.os,
        device_type=body.device_type,
        first_seen_at=now,
        last_seen_at=now,
    )


def update_device(device: Device, body: DeviceIn) -> None:
    """Set the name, os and device_type of a known computer. The last call wins (contract 5.3).

    Args:
        device: The stored computer.
        body: The registration: device_name, os and device_type.
    """
    device.display_name = body.device_name
    device.os = body.os
    device.device_type = body.device_type


def save_registration(session: Session, device_id: UUID, body: DeviceIn) -> bool:
    """Store one registration: update the computer when it is known, add it when it is not.

    Args:
        session: The database session.
        device_id: The id of the computer, from the path.
        body: The registration: device_name, os and device_type.

    Returns:
        True when this call added the computer, False when it was known before.

    Raises:
        IntegrityError: Another call added the same computer at the same moment.
    """
    device = session.get(Device, device_id)
    if device is not None:
        update_device(device, body)
        session.commit()
        return False
    session.add(new_device(device_id, body, utc_now()))
    session.commit()
    return True


@router.put("/devices/{device_id}", response_model=DeviceOut,
            dependencies=[Depends(require_client_key), Depends(require_json)])
def register_device(device_id: UUID, body: DeviceIn,
                    session: Annotated[Session, Depends(get_session)]) -> DeviceOut:
    """Register a computer or change its name (contract 5.3).

    Args:
        device_id: The id of the computer, from the path.
        body: The registration: device_name, os and device_type.
        session: The database session.

    Returns:
        The id, the name now, and created (true only when this call added the computer).
    """
    try:
        created = save_registration(session, device_id, body)
    except IntegrityError:
        # Two first calls came at the same moment and the other one added the row. This call
        # is then a repeat, so it updates the row and answers created false.
        session.rollback()
        created = save_registration(session, device_id, body)
    return DeviceOut(device_id=device_id, device_name=body.device_name, created=created)


@router.get("/devices", response_model=DevicesOut, dependencies=[Depends(require_admin)])
def list_devices(session: Annotated[Session, Depends(get_session)], location_id: int | None = None, include_archived: bool = False) -> DevicesOut:
    source = (devices_from_database(session, datetime.now(timezone.utc))
              if settings.dashboard_data_source == "database" else demo_devices())
    devices = [
        device for device in source
        if (location_id is None or device.location_id == location_id)
        and (include_archived or not device.archived)
    ]
    devices.sort(key=lambda device: (device.display_name or "", str(device.device_id)))
    return DevicesOut(devices=devices)
