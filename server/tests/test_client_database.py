# server/tests/test_client_database.py
"""PUT /api/devices, GET /api/settings and POST /api/ping read and write the database (W8).

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
from ecowattsense_server.models import Device, Setting, Tariff

DEVICE_ID = "5f0c1e7a-3b9d-4c1e-8a52-0d6e4f7b9a11"
DEVICE_PATH = f"/api/devices/{DEVICE_ID}"
DEVICE = {"device_name": "Lab 1 PC 03", "os": "windows", "device_type": "desktop"}
PING = {"device_id": DEVICE_ID, "unsent_reports": 0}


@pytest.fixture
def engine(tmp_path, monkeypatch):
    """A temporary SQLite file. init_db() and get_session() both use db.engine."""
    engine = create_engine(f"sqlite:///{tmp_path / 'test.db'}", connect_args={"check_same_thread": False})
    monkeypatch.setattr(db, "engine", engine)
    yield engine
    engine.dispose()


@pytest.fixture
def client(engine, monkeypatch):
    """A test client with the client key. Starting it creates and seeds the tables."""
    monkeypatch.setattr(settings, "client_key", "client-test-key")
    with TestClient(app) as test_client:
        test_client.headers["X-EWS-Key"] = "client-test-key"
        yield test_client


def devices_in(engine) -> list[Device]:
    """Read every row of the devices table."""
    with Session(engine) as session:
        return list(session.exec(select(Device)).all())


def set_value(engine, key: str, value: str) -> None:
    """Write one setting, as the admin code does: add the row or change it."""
    with Session(engine) as session:
        row = session.exec(select(Setting).where(Setting.key == key)).first() or Setting(key=key, value=value)
        row.value = value
        session.add(row)
        session.commit()


def test_put_creates_the_device_row(client, engine):
    response = client.put(DEVICE_PATH, json=DEVICE)
    assert response.status_code == 200
    assert response.json()["created"] is True
    (device,) = devices_in(engine)
    assert device.id == UUID(DEVICE_ID)
    assert (device.display_name, device.os, device.device_type) == ("Lab 1 PC 03", "windows", "desktop")
    assert device.first_seen_at == device.last_seen_at
    assert device.archived is False


def test_second_put_for_the_same_device(client, engine):
    assert client.put(DEVICE_PATH, json=DEVICE).json()["created"] is True
    first_seen = devices_in(engine)[0].first_seen_at
    changed = {"device_name": "Lab 2 PC 01", "os": "linux", "device_type": "laptop"}
    response = client.put(DEVICE_PATH, json=changed)
    assert response.status_code == 200
    # The name is stored only while it is empty (contract 5.3), so the first name stays.
    assert response.json() == {"device_id": DEVICE_ID, "device_name": "Lab 1 PC 03", "created": False}
    (device,) = devices_in(engine)
    assert (device.display_name, device.os, device.device_type) == ("Lab 1 PC 03", "linux", "laptop")
    assert device.first_seen_at == first_seen


def test_put_without_a_name_keeps_the_stored_name(client, engine):
    client.put(DEVICE_PATH, json=DEVICE)
    response = client.put(DEVICE_PATH, json={"os": "windows", "device_type": "desktop"})
    assert response.json() == {"device_id": DEVICE_ID, "device_name": "Lab 1 PC 03", "created": False}
    assert devices_in(engine)[0].display_name == "Lab 1 PC 03"


def test_settings_come_from_the_seeded_tables(client):
    assert client.get("/api/settings").json() == {
        "settings_version": 1, "report_interval_seconds": 300,
        "ping_interval_seconds": 60, "idle_threshold_seconds": 600,
        "unused_threshold_seconds": 3600, "no_data_after_cycles": 3,
        "tariff_valid_from": "2000-01-01T00:00:00Z",
        "price_gbp_per_kwh": 0.25, "co2_kg_per_kwh": 0.207,
    }


def test_a_changed_setting_shows_in_get_settings(client, engine):
    set_value(engine, "report_interval_seconds", "450")
    set_value(engine, "settings_version", "2")
    body = client.get("/api/settings").json()
    assert body["report_interval_seconds"] == 450
    assert body["settings_version"] == 2


def test_settings_send_the_tariff_in_force(client, engine):
    with Session(engine) as session:
        session.add(Tariff(valid_from=datetime(2026, 9, 14, tzinfo=timezone.utc), price_gbp_per_kwh=0.30, co2_kg_per_kwh=0.15))
        session.add(Tariff(valid_from=datetime(2999, 1, 1, tzinfo=timezone.utc), price_gbp_per_kwh=9.99, co2_kg_per_kwh=9.99))
        session.commit()
    body = client.get("/api/settings").json()
    assert body["tariff_valid_from"] == "2026-09-14T00:00:00Z"
    assert (body["price_gbp_per_kwh"], body["co2_kg_per_kwh"]) == (0.30, 0.15)


def test_ping_unknown_device_is_not_registered(client, engine):
    response = client.post("/api/ping", json=PING)
    assert response.status_code == 200
    assert response.json() == {"settings_version": 1, "registered": False, "commands": []}
    assert devices_in(engine) == []


def test_ping_after_put_is_registered(client):
    client.put(DEVICE_PATH, json=DEVICE)
    assert client.post("/api/ping", json=PING).json()["registered"] is True


def test_ping_sends_the_stored_settings_version(client, engine):
    set_value(engine, "settings_version", "7")
    assert client.post("/api/ping", json=PING).json()["settings_version"] == 7
