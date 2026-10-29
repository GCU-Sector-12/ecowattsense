import json

import pytest
from fastapi.testclient import TestClient

from ecowattsense_server.config import settings
from ecowattsense_server.main import app

DEVICE_ID = "5f0c1e7a-3b9d-4c1e-8a52-0d6e4f7b9a11"
DEVICE_PATH = f"/api/devices/{DEVICE_ID}"
PING = {"device_id": DEVICE_ID, "unsent_reports": 12}
DEVICE = {"device_name": " Lab 1 PC 03 ", "os": "windows", "device_type": "desktop"}


@pytest.fixture
def client(monkeypatch):
    monkeypatch.setattr(settings, "client_key", "client-test-key")
    with TestClient(app) as test_client:
        test_client.headers["X-EWS-Key"] = "client-test-key"
        yield test_client


@pytest.mark.parametrize("key", [None, "wrong"])
@pytest.mark.parametrize("method,path,body", [
    ("GET", "/api/settings", None), ("POST", "/api/ping", PING), ("PUT", DEVICE_PATH, DEVICE),
])
def test_key_required(client, key, method, path, body):
    client.headers.pop("X-EWS-Key")
    if key:
        client.headers["X-EWS-Key"] = key
    assert client.request(method, path, json=body).status_code == 401


def test_settings(client, monkeypatch):
    monkeypatch.setattr(settings, "report_interval_seconds", 450)
    response = client.get("/api/settings")
    assert response.status_code == 200
    assert response.json() == {
        "settings_version": 1, "report_interval_seconds": 450,
        "ping_interval_seconds": 60, "idle_threshold_seconds": 600,
        "unused_threshold_seconds": 3600, "no_data_after_cycles": 3,
        "tariff_valid_from": "2000-01-01T00:00:00Z",
        "price_gbp_per_kwh": 0.25, "co2_kg_per_kwh": 0.207,
    }


def test_ping(client):
    response = client.post("/api/ping", json={**PING, "acks": [{"command_id": 41, "result": "done"}]})
    assert response.status_code == 200
    assert response.json() == {"settings_version": 1, "registered": True, "commands": []}
    assert client.post("/api/ping", json=PING).status_code == 200


@pytest.mark.parametrize("changes", [
    {"device_id": "invalid"}, {"unsent_reports": -1}, {"unsent_reports": True},
    {"unsent_reports": "12"}, {"extra": 1}, {"acks": None},
    {"acks": [{"command_id": "41", "result": "done"}]},
    {"acks": [{"command_id": 41, "result": "unknown"}]},
    {"acks": [{"command_id": 41, "result": "done", "extra": True}]},
])
def test_ping_validation(client, changes):
    assert client.post("/api/ping", json={**PING, **changes}).status_code == 422


@pytest.mark.parametrize("name,expected", [(" Lab 1 PC 03 ", "Lab 1 PC 03"), ("", None), ("   ", None), (None, None)])
def test_device_name(client, name, expected):
    for _ in range(2):
        response = client.put(DEVICE_PATH, json={**DEVICE, "device_name": name})
        assert response.status_code == 200
        assert response.json() == {"device_id": DEVICE_ID, "device_name": expected, "created": True}


def test_optional_device_name(client):
    assert client.put(DEVICE_PATH, json={"os": "linux", "device_type": "laptop"}).json()["device_name"] is None


@pytest.mark.parametrize("changes", [
    {"device_name": "x" * 61}, {"device_name": "\tname"}, {"device_name": "a\x7fb"},
    {"os": "unknown"}, {"device_type": "Desktop"}, {"device_type": "a" * 41},
    {"device_type": "a-b"}, {"extra": True},
])
def test_device_validation(client, changes):
    assert client.put(DEVICE_PATH, json={**DEVICE, **changes}).status_code == 422


@pytest.mark.parametrize("method,path,body", [("POST", "/api/ping", PING), ("PUT", DEVICE_PATH, DEVICE)])
def test_missing_fields_and_content_type(client, method, path, body):
    assert client.request(method, path, json={}).status_code == 422
    assert client.request(method, path).status_code == 422
    assert client.request(method, path, content=json.dumps(body)).status_code == 422
    assert client.request(method, path, content=json.dumps(body), headers={"Content-Type": "text/plain"}).status_code == 422


def test_invalid_path_uuid(client):
    assert client.put("/api/devices/not-a-uuid", json=DEVICE).status_code == 422
