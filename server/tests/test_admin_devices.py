# server/tests/test_admin_devices.py
"""PATCH /api/admin/devices/{device_id} (contract 5.11) and the PUT name rule (contract 5.3).

Every test runs on a temporary SQLite file, never the real database.
"""
from datetime import datetime, timezone
from uuid import UUID

import pytest
from fastapi.testclient import TestClient
from sqlmodel import Session, create_engine, select

from ecowattsense_server import db
from ecowattsense_server.config import settings
from ecowattsense_server.main import app
from ecowattsense_server.models import Device, Location

DEVICE_ID = "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12"
OTHER_ID = "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a13"
PATH = f"/api/admin/devices/{DEVICE_ID}"
LIST_KEYS = {"device_id", "display_name", "location_id", "location_name", "building", "os",
             "device_type", "state", "state_name", "last_seen_at", "first_seen_at", "archived"}


@pytest.fixture
def engine(tmp_path, monkeypatch):
    """A temporary SQLite file. init_db() and get_session() both use db.engine."""
    engine = create_engine(f"sqlite:///{tmp_path / 'test.db'}", connect_args={"check_same_thread": False})
    monkeypatch.setattr(db, "engine", engine)
    yield engine
    engine.dispose()


@pytest.fixture
def client(engine, monkeypatch):
    """A client logged in as the admin, with two computers and one room in the database."""
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    monkeypatch.setattr(settings, "client_key", "client-test-key")
    monkeypatch.setattr(settings, "dashboard_data_source", "database")
    with TestClient(app) as test_client:
        add_rows(engine)
        response = test_client.post("/api/login", json={"username": "admin", "password": "test-password"})
        assert response.status_code == 200
        yield test_client


def add_rows(engine) -> None:
    """Add the room, one computer with a name and one computer with no name."""
    seen = datetime(2026, 10, 12, 9, 0, tzinfo=timezone.utc)
    with Session(engine) as session:
        session.add(Location(id=1, name="Lab 1", building="George Moore"))
        session.add(Device(id=UUID(DEVICE_ID), display_name="Lab 1 PC 03", os="windows",
                           device_type="laptop", first_seen_at=seen, last_seen_at=seen))
        session.add(Device(id=UUID(OTHER_ID), display_name=None, os="linux",
                           device_type="desktop", first_seen_at=seen, last_seen_at=seen))
        session.commit()


def stored(engine, device_id: str = DEVICE_ID) -> Device:
    """Read one computer from the database."""
    with Session(engine) as session:
        return session.exec(select(Device).where(Device.id == UUID(device_id))).one()


def listed_ids(client, query: str = "") -> set[str]:
    """The ids that GET /api/devices lists."""
    return {row["device_id"] for row in client.get(f"/api/devices{query}").json()["devices"]}


@pytest.mark.parametrize("body,field,value", [
    ({"display_name": "Renamed"}, "display_name", "Renamed"),
    ({"location_id": 1}, "location_id", 1),
    ({"archived": True}, "archived", True),
])
def test_each_field_alone(client, engine, body, field, value):
    response = client.patch(PATH, json=body)
    assert response.status_code == 200
    assert response.json()[field] == value
    assert getattr(stored(engine), field) == value


def test_all_three_fields_together(client, engine):
    body = {"display_name": "Lab 1 PC 04", "location_id": 1, "archived": True}
    response = client.patch(PATH, json=body)
    assert response.status_code == 200
    row = response.json()
    assert (row["display_name"], row["location_id"], row["archived"]) == ("Lab 1 PC 04", 1, True)
    assert (row["location_name"], row["building"]) == ("Lab 1", "George Moore")
    device = stored(engine)
    assert (device.display_name, device.location_id, device.archived) == ("Lab 1 PC 04", 1, True)


def test_answer_has_the_shape_of_a_list_element(client):
    client.patch(PATH, json={"location_id": 1})
    answer = client.patch(PATH, json={"display_name": "Same"}).json()
    assert set(answer) == LIST_KEYS
    listed = {row["device_id"]: row for row in client.get("/api/devices").json()["devices"]}
    assert listed[DEVICE_ID] == answer


def test_cookie_required(client):
    client.cookies.clear()
    assert client.patch(PATH, json={"archived": True}).status_code == 401
    assert client.patch(PATH, json={"archived": True}, headers={"X-EWS-Key": "client-test-key"}).status_code == 401


def test_unknown_device_is_404(client):
    response = client.patch("/api/admin/devices/00000000-0000-0000-0000-000000000000", json={"archived": True})
    assert response.status_code == 404


@pytest.mark.parametrize("body", [
    {"archived": "yes"},
    {"archived": 1},
    {"location_id": "1"},
    {"location_id": 1.5},
    {"display_name": 5},
    {"unknown": 1},
    {"archived": True, "unknown": 1},
    {},
    {"display_name": "x" * 61},
    {"location_id": 999},
])
def test_wrong_body_is_422(client, engine, body):
    before = stored(engine)
    assert client.patch(PATH, json=body).status_code == 422
    assert stored(engine) == before


def test_name_of_60_characters_is_accepted(client):
    assert client.patch(PATH, json={"display_name": "x" * 60}).status_code == 200


def test_null_clears_the_name_and_the_room(client, engine):
    client.patch(PATH, json={"location_id": 1})
    response = client.patch(PATH, json={"display_name": None, "location_id": None})
    assert response.status_code == 200
    row = response.json()
    assert (row["display_name"], row["location_id"], row["location_name"], row["building"]) == (None, None, None, None)
    device = stored(engine)
    assert (device.display_name, device.location_id) == (None, None)


def test_a_missing_field_stays_as_it_was(client, engine):
    client.patch(PATH, json={"location_id": 1})
    client.patch(PATH, json={"archived": True})
    device = stored(engine)
    assert (device.display_name, device.location_id, device.archived) == ("Lab 1 PC 03", 1, True)


def test_archived_hides_and_unarchived_shows(client):
    assert DEVICE_ID in listed_ids(client)
    client.patch(PATH, json={"archived": True})
    assert DEVICE_ID not in listed_ids(client)
    assert DEVICE_ID in listed_ids(client, "?include_archived=true")
    client.patch(PATH, json={"archived": False})
    assert DEVICE_ID in listed_ids(client)


def test_the_same_patch_twice_gives_200_twice(client):
    body = {"display_name": "Twice", "archived": True}
    first = client.patch(PATH, json=body)
    second = client.patch(PATH, json=body)
    assert (first.status_code, second.status_code) == (200, 200)
    assert first.json() == second.json()


def test_patch_works_in_stub_mode_too(client, monkeypatch):
    monkeypatch.setattr(settings, "dashboard_data_source", "stub")
    assert client.patch(PATH, json={"archived": True}).status_code == 200


def put_body(name: str | None) -> dict:
    """A registration body for PUT /api/devices/{device_id}."""
    return {"device_name": name, "os": "windows", "device_type": "laptop"}


def test_admin_name_is_not_overwritten_by_put(client, engine):
    client.patch(PATH, json={"display_name": "Admin name"})
    response = client.put(f"/api/devices/{DEVICE_ID}", json=put_body("Client name"),
                          headers={"X-EWS-Key": "client-test-key"})
    assert response.status_code == 200
    assert response.json() == {"device_id": DEVICE_ID, "device_name": "Admin name", "created": False}
    assert stored(engine).display_name == "Admin name"


def test_put_sets_the_name_while_it_is_empty(client, engine):
    response = client.put(f"/api/devices/{OTHER_ID}", json=put_body("Client name"),
                          headers={"X-EWS-Key": "client-test-key"})
    assert response.json() == {"device_id": OTHER_ID, "device_name": "Client name", "created": False}
    assert stored(engine, OTHER_ID).display_name == "Client name"


def test_put_after_the_admin_cleared_the_name_sets_it(client, engine):
    client.patch(PATH, json={"display_name": None})
    client.put(f"/api/devices/{DEVICE_ID}", json=put_body("Client name"), headers={"X-EWS-Key": "client-test-key"})
    assert stored(engine).display_name == "Client name"
