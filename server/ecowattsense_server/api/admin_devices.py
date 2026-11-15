"""PATCH /api/admin/devices/{device_id}: the admin changes a computer (contract 5.11).

The dashboard calls it to rename a computer, put it in a room or archive it. It needs the
admin cookie. It always works on the database, whatever dashboard_data_source says, because a
change has to be stored. The answer is built by the same code as GET /api/devices.
"""
from datetime import datetime, timezone
from typing import Annotated
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException
from fastapi.exceptions import RequestValidationError
from sqlmodel import Session

from ..auth import require_admin
from ..dashboard_store import devices_from_database
from ..db import get_session
from ..models import Device, Location
from ..schemas import AdminDeviceChange, DashboardDevice

router = APIRouter(prefix="/api/admin", tags=["admin"])


def find_device(session: Session, device_id: UUID) -> Device:
    """Read one computer.

    Args:
        session: The database session.
        device_id: The id of the computer, from the path.

    Returns:
        The stored computer.

    Raises:
        HTTPException: 404, there is no computer with this id.
    """
    device = session.get(Device, device_id)
    if device is None:
        raise HTTPException(404, "Device not found")
    return device


def require_known_room(session: Session, location_id: int | None) -> None:
    """Check that a room id exists. Null is fine, it means no room.

    The check needs the database, so it cannot live in the request model.

    Args:
        session: The database session.
        location_id: The room from the body, or None.

    Raises:
        RequestValidationError: 422, the id is not in the locations table.
    """
    if location_id is None:
        return
    if session.get(Location, location_id) is not None:
        return
    raise RequestValidationError([{
        "type": "value_error", "loc": ("body", "location_id"),
        "msg": "location_id is not an id in locations", "input": location_id,
    }])


def apply_change(device: Device, body: AdminDeviceChange) -> None:
    """Copy the fields that the body sent onto the computer. Other fields stay as they were.

    Args:
        device: The stored computer.
        body: The change. Its model_fields_set names the fields that were sent.
    """
    for name in body.model_fields_set:
        setattr(device, name, getattr(body, name))


def read_changed_device(session: Session, device_id: UUID) -> DashboardDevice:
    """Build the computer in the shape of one element of GET /api/devices.

    Args:
        session: The database session.
        device_id: The id of the computer.

    Returns:
        The computer as the dashboard list shows it, archived or not.
    """
    now = datetime.now(timezone.utc)
    (row,) = [item for item in devices_from_database(session, now) if item.device_id == device_id]
    return row


@router.patch("/devices/{device_id}", response_model=DashboardDevice, dependencies=[Depends(require_admin)])
def change_device(device_id: UUID, body: AdminDeviceChange,
                  session: Annotated[Session, Depends(get_session)]) -> DashboardDevice:
    """Change the name, the room or the archived flag of a computer (contract 5.11).

    A repeat of the same call gives the same answer. When the write fails the database
    error is not caught: the client gets 500 and nothing is stored.

    Args:
        device_id: The id of the computer, from the path.
        body: The fields to change.
        session: The database session.

    Returns:
        The computer as it is now.

    Raises:
        HTTPException: 404, unknown device.
        RequestValidationError: 422, the room does not exist.
    """
    device = find_device(session, device_id)
    require_known_room(session, body.location_id)
    apply_change(device, body)
    session.commit()
    return read_changed_device(session, device_id)
