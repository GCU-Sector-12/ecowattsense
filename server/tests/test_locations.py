# server/tests/test_locations.py
"""GET /api/locations, POST and PATCH /api/admin/locations (contract 5.12 to 5.14).

Every test runs on a temporary SQLite file, never the real database.
"""
import pytest
from fastapi.testclient import TestClient
from sqlmodel import Session, create_engine, select

from ecowattsense_server import db
from ecowattsense_server.config import settings
from ecowattsense_server.main import app
from ecowattsense_server.models import Location

ADMIN = {"username": "admin", "password": "test-password"}


@pytest.fixture
def engine(tmp_path, monkeypatch):
    """A temporary SQLite file. init_db() and get_session() both use db.engine."""
    engine = create_engine(f"sqlite:///{tmp_path / 'test.db'}", connect_args={"check_same_thread": False})
    monkeypatch.setattr(db, "engine", engine)
    yield engine
    engine.dispose()


@pytest.fixture
def anonymous(engine, monkeypatch):
    """A client with no cookie, on an empty database."""
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    monkeypatch.setattr(settings, "dashboard_data_source", "database")
    with TestClient(app) as test_client:
        yield test_client


@pytest.fixture
def client(anonymous):
    """A client logged in as the admin, on an empty database."""
    assert anonymous.post("/api/login", json=ADMIN).status_code == 200
    return anonymous


def add_room(engine, name: str, building: str | None = None) -> int:
    """Store a room and give back its id."""
    with Session(engine) as session:
        room = Location(name=name, building=building)
        session.add(room)
        session.commit()
        session.refresh(room)
        return room.id


def stored_rooms(engine) -> list[tuple[str, str | None]]:
    """Every stored room as (name, building)."""
    with Session(engine) as session:
        return [(row.name, row.building) for row in session.exec(select(Location)).all()]


def test_get_empty(client):
    response = client.get("/api/locations")
    assert response.status_code == 200
    assert response.json() == {"locations": []}


def test_get_sorted_by_name_then_building(client, engine):
    add_room(engine, "Lab 2", "B")
    add_room(engine, "Lab 1", "George Moore")
    add_room(engine, "Annex", None)
    names = [row["name"] for row in client.get("/api/locations").json()["locations"]]
    assert names == ["Annex", "Lab 1", "Lab 2"]


def test_get_needs_cookie(anonymous):
    assert anonymous.get("/api/locations").status_code == 401


def test_post_creates_room(client, engine):
    response = client.post("/api/admin/locations", json={"name": "Lab 3", "building": "George Moore"})
    assert response.status_code == 201
    body = response.json()
    assert (body["name"], body["building"]) == ("Lab 3", "George Moore")
    assert isinstance(body["id"], int)
    assert stored_rooms(engine) == [("Lab 3", "George Moore")]
    assert client.get("/api/locations").json()["locations"] == [body]


def test_post_trims(client):
    response = client.post("/api/admin/locations", json={"name": "  Lab 3  ", "building": "  Cowcaddens "})
    assert response.status_code == 201
    assert (response.json()["name"], response.json()["building"]) == ("Lab 3", "Cowcaddens")


@pytest.mark.parametrize("building", ["", "   ", None])
def test_post_blank_building_is_null(client, building):
    response = client.post("/api/admin/locations", json={"name": "Lab 3", "building": building})
    assert response.status_code == 201
    assert response.json()["building"] is None


def test_post_without_building(client):
    response = client.post("/api/admin/locations", json={"name": "Lab 3"})
    assert response.status_code == 201
    assert response.json()["building"] is None


def test_post_needs_cookie(anonymous):
    assert anonymous.post("/api/admin/locations", json={"name": "Lab 3"}).status_code == 401


@pytest.mark.parametrize("body", [
    {},
    {"building": "George Moore"},
    {"name": ""},
    {"name": "   "},
    {"name": "x" * 61},
    {"name": "Lab 3", "building": "x" * 61},
    {"name": "Lab 3", "floor": 2},
    {"name": 5},
])
def test_post_bad_body(client, engine, body):
    response = client.post("/api/admin/locations", json=body)
    assert response.status_code == 422
    assert stored_rooms(engine) == []


def test_post_longest_name_is_fine(client):
    assert client.post("/api/admin/locations", json={"name": "x" * 60}).status_code == 201


@pytest.mark.parametrize("building", [None, "Other"])
def test_post_duplicate_name_is_409(client, engine, building):
    add_room(engine, "Lab 1", "George Moore")
    response = client.post("/api/admin/locations", json={"name": " Lab 1 ", "building": building})
    assert response.status_code == 409
    assert stored_rooms(engine) == [("Lab 1", "George Moore")]


def test_patch_changes_fields(client, engine):
    room_id = add_room(engine, "Lab 1", "George Moore")
    response = client.patch(f"/api/admin/locations/{room_id}", json={"name": " Lab One "})
    assert response.status_code == 200
    assert response.json() == {"id": room_id, "name": "Lab One", "building": "George Moore"}
    response = client.patch(f"/api/admin/locations/{room_id}", json={"building": "  "})
    assert response.json() == {"id": room_id, "name": "Lab One", "building": None}
    assert stored_rooms(engine) == [("Lab One", None)]


def test_patch_same_name_is_fine(client, engine):
    room_id = add_room(engine, "Lab 1", "George Moore")
    response = client.patch(f"/api/admin/locations/{room_id}", json={"name": "Lab 1", "building": "Other"})
    assert response.status_code == 200
    assert response.json()["building"] == "Other"


def test_patch_unknown_id_is_404(client):
    assert client.patch("/api/admin/locations/999", json={"name": "X"}).status_code == 404


@pytest.mark.parametrize("body", [
    {},
    {"floor": 2},
    {"name": None},
    {"name": ""},
    {"name": "x" * 61},
    {"building": "x" * 61},
])
def test_patch_bad_body(client, engine, body):
    room_id = add_room(engine, "Lab 1", "George Moore")
    response = client.patch(f"/api/admin/locations/{room_id}", json=body)
    assert response.status_code == 422
    assert stored_rooms(engine) == [("Lab 1", "George Moore")]


def test_patch_duplicate_name_is_409(client, engine):
    add_room(engine, "Lab 1", None)
    room_id = add_room(engine, "Lab 2", None)
    response = client.patch(f"/api/admin/locations/{room_id}", json={"name": "Lab 1"})
    assert response.status_code == 409
    assert ("Lab 2", None) in stored_rooms(engine)


def test_patch_needs_cookie(anonymous, engine):
    room_id = add_room(engine, "Lab 1")
    assert anonymous.patch(f"/api/admin/locations/{room_id}", json={"name": "X"}).status_code == 401
