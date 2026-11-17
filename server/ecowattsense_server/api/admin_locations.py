"""The rooms endpoints: GET /api/locations, POST and PATCH /api/admin/locations (contract 5.12 to 5.14).

The dashboard uses them to fill a room picker and to add or rename rooms. All three need the
admin cookie. The writes always go to the database, whatever dashboard_data_source says,
because a change has to be stored. GET reads like GET /api/devices: the database, or the
demo rooms in stub mode. There is no delete endpoint yet.
"""
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.exc import IntegrityError
from sqlmodel import Session, select

from ..auth import require_admin
from ..config import settings
from ..db import get_session
from ..demo_data import demo_devices
from ..models import Location
from ..schemas import LocationChange, LocationIn, LocationOut, LocationsOut

router = APIRouter(tags=["locations"], dependencies=[Depends(require_admin)])


def demo_locations() -> list[LocationOut]:
    """Build the rooms of the demo data, for stub mode.

    Returns:
        One room for each distinct location_id that the demo computers use.
    """
    rooms = {
        device.location_id: LocationOut(id=device.location_id, name=device.location_name, building=device.building)
        for device in demo_devices()
        if device.location_id is not None
    }
    return list(rooms.values())


def sort_rooms(rooms: list[LocationOut]) -> list[LocationOut]:
    """Sort rooms by name, then by building. A missing building sorts first.

    Args:
        rooms: The rooms in any order.

    Returns:
        A new sorted list.
    """
    return sorted(rooms, key=lambda room: (room.name, room.building or ""))


def require_free_name(session: Session, name: str, own_id: int | None) -> None:
    """Check that no other room has this name.

    The name is unique in the table (database brief 5.1), whatever the building.

    Args:
        session: The database session.
        name: The trimmed name from the body.
        own_id: The id of the room that is being changed, or None for a new room.

    Raises:
        HTTPException: 409, another room has this name.
    """
    other = session.exec(select(Location).where(Location.name == name)).first()
    if other is None or other.id == own_id:
        return
    raise HTTPException(409, "A room with this name exists")


def commit_room(session: Session) -> None:
    """Save the change. Two calls at the same time can pass the name check, so the unique index decides.

    Args:
        session: The database session.

    Raises:
        HTTPException: 409, the unique index on the name refused the write.
    """
    try:
        session.commit()
    except IntegrityError:
        session.rollback()
        raise HTTPException(409, "A room with this name exists")


def room_out(room: Location) -> LocationOut:
    """Turn a stored room into the answer shape.

    Args:
        room: A saved room.

    Returns:
        The room as the contract shows it.
    """
    return LocationOut(id=room.id, name=room.name, building=room.building)


@router.get("/api/locations", response_model=LocationsOut)
def list_locations(session: Annotated[Session, Depends(get_session)]) -> LocationsOut:
    """List the rooms, sorted by name and then by building (contract 5.12).

    Args:
        session: The database session.

    Returns:
        The rooms. The list can be empty.
    """
    if settings.dashboard_data_source != "database":
        return LocationsOut(locations=sort_rooms(demo_locations()))
    rooms = [room_out(room) for room in session.exec(select(Location)).all()]
    return LocationsOut(locations=sort_rooms(rooms))


@router.post("/api/admin/locations", response_model=LocationOut, status_code=201)
def add_location(body: LocationIn, session: Annotated[Session, Depends(get_session)]) -> LocationOut:
    """Add a room (contract 5.13).

    Args:
        body: The name and the optional building.
        session: The database session.

    Returns:
        The new room.

    Raises:
        HTTPException: 409, a room with this name exists.
    """
    require_free_name(session, body.name, None)
    room = Location(name=body.name, building=body.building)
    session.add(room)
    commit_room(session)
    session.refresh(room)
    return room_out(room)


@router.patch("/api/admin/locations/{location_id}", response_model=LocationOut)
def change_location(location_id: int, body: LocationChange,
                    session: Annotated[Session, Depends(get_session)]) -> LocationOut:
    """Change the name or the building of a room (contract 5.14).

    Args:
        location_id: The id of the room, from the path.
        body: The fields to change.
        session: The database session.

    Returns:
        The room as it is now.

    Raises:
        HTTPException: 404 for an unknown room, 409 when another room has the name.
    """
    room = session.get(Location, location_id)
    if room is None:
        raise HTTPException(404, "Location not found")
    if "name" in body.model_fields_set:
        # Check before the change: a query after it would flush the new name and hit the unique index.
        require_free_name(session, body.name, room.id)
    for field in body.model_fields_set:
        setattr(room, field, getattr(body, field))
    commit_room(session)
    session.refresh(room)
    return room_out(room)
