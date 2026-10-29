import pytest
from fastapi.testclient import TestClient

from ecowattsense_server.api import devices
from ecowattsense_server.config import settings
from ecowattsense_server.main import app


@pytest.fixture
def client(monkeypatch):
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    with TestClient(app) as test_client:
        response = test_client.post("/api/login", json={"username": "admin", "password": "test-password"})
        assert response.status_code == 200
        yield test_client


def test_devices_shape(client):
    response = client.get("/api/devices")
    assert response.status_code == 200
    rows = response.json()["devices"]
    assert len(rows) == 3
    assert {(r["state"], r["state_name"]) for r in rows} == {(2, "active"), (1, "idle"), (0, "unused")}
    assert rows == sorted(rows, key=lambda r: (r["display_name"] or "", r["device_id"]))
    assert all(set(row) == {"device_id", "display_name", "location_id", "location_name", "building",
        "os", "device_type", "state", "state_name", "last_seen_at", "first_seen_at", "archived"} for row in rows)


def test_location_and_empty_list(client):
    assert len(client.get("/api/devices?location_id=1").json()["devices"]) == 2
    assert client.get("/api/devices?location_id=999").json() == {"devices": []}


def test_archive_filter_and_tie_sort(client, monkeypatch):
    sample = devices.demo_devices()
    sample[0].archived = True
    for row in sample:
        row.display_name = None
    monkeypatch.setattr(devices, "demo_devices", lambda: list(reversed(sample)))
    assert len(client.get("/api/devices").json()["devices"]) == 2
    rows = client.get("/api/devices?include_archived=true").json()["devices"]
    assert len(rows) == 3
    assert [r["device_id"] for r in rows] == sorted(r["device_id"] for r in rows)


@pytest.mark.parametrize("query", ["location_id=abc", "location_id=1.5", "include_archived=invalid"])
def test_query_validation(client, query):
    assert client.get(f"/api/devices?{query}").status_code == 422


def test_cookie_required(client):
    client.cookies.clear()
    assert client.get("/api/devices").status_code == 401
    assert client.get("/api/devices", headers={"X-EWS-Key": "anything"}).status_code == 401
    client.cookies.set("ews_session", "forged")
    assert client.get("/api/devices").status_code == 401
