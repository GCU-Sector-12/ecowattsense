# server/tests/test_client_rename.py
"""A client may rename a computer that it named itself, never one the admin named (EWS_API-24).

Also tests the upgrade step of init_db for a database made before the column name_source existed.
Every test runs on a temporary SQLite file, never the real database.
"""
from uuid import UUID

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import text
from sqlmodel import Session, create_engine, select

from ecowattsense_server import db
from ecowattsense_server.config import settings
from ecowattsense_server.main import app
from ecowattsense_server.models import Device

DEVICE_ID = "5f0c1e7a-3b9d-4c1e-8a52-0d6e4f7b9a11"
PATH = f"/api/devices/{DEVICE_ID}"
ADMIN_PATH = f"/api/admin/devices/{DEVICE_ID}"


def body(name: str | None) -> dict:
    """Make a registration body with the given name."""
    return {"device_name": name, "os": "windows", "device_type": "desktop"}


@pytest.fixture
def engine(tmp_path, monkeypatch):
    """A temporary SQLite file. init_db() and get_session() both use db.engine."""
    engine = create_engine(f"sqlite:///{tmp_path / 'test.db'}", connect_args={"check_same_thread": False})
    monkeypatch.setattr(db, "engine", engine)
    yield engine
    engine.dispose()


@pytest.fixture
def client(engine, monkeypatch):
    """A test client that has the client key and is logged in as the admin."""
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    monkeypatch.setattr(settings, "client_key", "client-test-key")
    monkeypatch.setattr(settings, "dashboard_data_source", "database")
    with TestClient(app) as test_client:
        test_client.headers["X-EWS-Key"] = "client-test-key"
        login = test_client.post("/api/login", json={"username": "admin", "password": "test-password"})
        assert login.status_code == 200
        yield test_client


def stored(engine) -> Device:
    """Read the one computer of the test."""
    with Session(engine) as session:
        return session.exec(select(Device).where(Device.id == UUID(DEVICE_ID))).one()


def test_new_computer_with_a_name_is_marked_client(client, engine):
    client.put(PATH, json=body("Lab 1"))
    assert (stored(engine).display_name, stored(engine).name_source) == ("Lab 1", "client")


def test_new_computer_without_a_name_has_no_source(client, engine):
    client.put(PATH, json=body(None))
    assert (stored(engine).display_name, stored(engine).name_source) == (None, None)


def test_client_renames_a_client_name(client, engine):
    client.put(PATH, json=body("Lab 1"))
    response = client.put(PATH, json=body("Lab 2"))
    assert response.json() == {"device_id": DEVICE_ID, "device_name": "Lab 2", "created": False}
    assert (stored(engine).display_name, stored(engine).name_source) == ("Lab 2", "client")


def test_client_cannot_rename_an_admin_name(client, engine):
    client.put(PATH, json=body("Lab 1"))
    client.patch(ADMIN_PATH, json={"display_name": "Admin name"})
    response = client.put(PATH, json=body("Lab 2"))
    assert response.json() == {"device_id": DEVICE_ID, "device_name": "Admin name", "created": False}
    assert (stored(engine).display_name, stored(engine).name_source) == ("Admin name", "admin")


def test_empty_name_clears_a_client_name(client, engine):
    client.put(PATH, json=body("Lab 1"))
    response = client.put(PATH, json=body(None))
    assert response.json()["device_name"] is None
    assert (stored(engine).display_name, stored(engine).name_source) == (None, None)


def test_empty_name_keeps_an_admin_name(client, engine):
    client.put(PATH, json=body(None))
    client.patch(ADMIN_PATH, json={"display_name": "Admin name"})
    response = client.put(PATH, json=body(None))
    assert response.json()["device_name"] == "Admin name"
    assert stored(engine).name_source == "admin"


def test_admin_renames_a_client_name(client, engine):
    client.put(PATH, json=body("Lab 1"))
    response = client.patch(ADMIN_PATH, json={"display_name": "Admin name"})
    assert response.status_code == 200
    assert (stored(engine).display_name, stored(engine).name_source) == ("Admin name", "admin")


def test_admin_clears_the_name_then_the_client_sets_it_again(client, engine):
    client.put(PATH, json=body("Lab 1"))
    client.patch(ADMIN_PATH, json={"display_name": "Admin name"})
    client.patch(ADMIN_PATH, json={"display_name": None})
    assert (stored(engine).display_name, stored(engine).name_source) == (None, None)
    response = client.put(PATH, json=body("Lab 3"))
    assert response.json()["device_name"] == "Lab 3"
    assert (stored(engine).display_name, stored(engine).name_source) == ("Lab 3", "client")


def test_admin_change_without_a_name_keeps_the_source(client, engine):
    client.put(PATH, json=body("Lab 1"))
    client.patch(ADMIN_PATH, json={"archived": True})
    assert (stored(engine).display_name, stored(engine).name_source) == ("Lab 1", "client")


OLD_TABLE = """CREATE TABLE devices (
    id CHAR(32) NOT NULL PRIMARY KEY, display_name VARCHAR, location_id INTEGER, machine_hash VARCHAR,
    os VARCHAR NOT NULL, device_type VARCHAR NOT NULL, first_seen_at DATETIME NOT NULL,
    last_seen_at DATETIME NOT NULL, archived BOOLEAN NOT NULL)"""
OLD_ROWS = """INSERT INTO devices (id, display_name, os, device_type, first_seen_at, last_seen_at, archived) VALUES
    ('a1', 'Named', 'linux', 'desktop', '2026-10-01 00:00:00', '2026-10-01 00:00:00', 0),
    ('a2', NULL, 'linux', 'desktop', '2026-10-01 00:00:00', '2026-10-01 00:00:00', 0)"""


def sources(engine) -> list:
    """Read name_source of the two old rows, ordered by id."""
    with engine.connect() as connection:
        return list(connection.execute(text("SELECT id, name_source FROM devices ORDER BY id")))


def test_upgrade_adds_the_column_and_marks_named_rows(engine):
    with engine.begin() as connection:
        connection.execute(text(OLD_TABLE))
        connection.execute(text(OLD_ROWS))
    db.init_db()
    assert sources(engine) == [("a1", "client"), ("a2", None)]


def test_upgrade_run_twice_changes_nothing(engine):
    with engine.begin() as connection:
        connection.execute(text(OLD_TABLE))
        connection.execute(text(OLD_ROWS))
        connection.execute(text("ALTER TABLE devices ADD COLUMN name_source TEXT"))
        connection.execute(text("UPDATE devices SET name_source = 'admin' WHERE id = 'a1'"))
    db.init_db()
    db.init_db()
    assert sources(engine) == [("a1", "admin"), ("a2", None)]


def test_upgrade_on_a_new_database_does_nothing(engine):
    db.init_db()
    db.init_db()
    assert sources(engine) == []
