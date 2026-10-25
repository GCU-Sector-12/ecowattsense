from datetime import datetime, timedelta, timezone
from uuid import uuid4

import pytest
from fastapi.testclient import TestClient

from ecowattsense_server.api import history
from ecowattsense_server.config import settings
from ecowattsense_server.dashboard_totals import report_totals
from ecowattsense_server.demo_data import demo_devices, demo_history, demo_tariffs
from ecowattsense_server.main import app

DEVICE_ID = demo_devices()[0].device_id
PATH = f"/api/devices/{DEVICE_ID}/history"


@pytest.fixture
def client(monkeypatch):
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    monkeypatch.setattr(settings, "dashboard_data_source", "stub")
    with TestClient(app) as test_client:
        assert test_client.post("/api/login", json={"username": "admin", "password": "test-password"}).status_code == 200
        yield test_client


def test_history_range_and_report_time_tariffs(client):
    response = client.get(PATH, params={"from": "2026-10-19T13:55:00Z", "to": "2026-10-19T14:05:00Z"})
    assert response.status_code == 200
    data = response.json()
    assert set(data) == {"device_id", "display_name", "from", "to", "reports", "totals"}
    assert len(data["reports"]) == 2
    assert data["reports"][0]["timestamp"] == "2026-10-19T13:55:00Z"
    assert set(data["reports"][0]) == {"timestamp", "interval_seconds", "state", "idle_seconds",
        "cpu_percent", "screen_locked", "on_battery", "energy_uj", "energy_source"}
    assert data["totals"] == pytest.approx({
        "active_time_seconds": 300, "idle_time_seconds": 120, "unused_time_seconds": 0,
        "energy_uj": 11_400_000_000, "cost_gbp": 9e9 / 3.6e12 * .20 + 2.4e9 / 3.6e12 * .25,
        "co2_kg": 9e9 / 3.6e12 * .25 + 2.4e9 / 3.6e12 * .207,
    })


def test_empty_history(client):
    response = client.get(PATH, params={"from": "2026-10-20T00:00:00Z", "to": "2026-10-21T00:00:00Z"})
    assert response.status_code == 200
    assert response.json()["reports"] == []
    assert all(value == 0 for value in response.json()["totals"].values())


def test_default_range(client, monkeypatch):
    now = datetime(2026, 10, 20, tzinfo=timezone.utc)
    monkeypatch.setattr(history, "utc_now", lambda: now)
    data = client.get(PATH).json()
    assert data["to"] == "2026-10-20T00:00:00Z"
    assert data["from"] == "2026-10-19T00:00:00Z"


@pytest.mark.parametrize("start,end", [
    ("2026-09-01T00:00:00Z", "2026-10-03T00:00:00Z"),
    ("2026-10-20T00:00:00Z", "2026-10-19T00:00:00Z"),
    ("2026-10-20T00:00:00Z", "2026-10-20T00:00:00Z"),
    ("2026-10-19T00:00:00", "2026-10-20T00:00:00Z"),
    ("invalid", "2026-10-20T00:00:00Z"),
])
def test_invalid_ranges(client, start, end):
    response = client.get(PATH, params={"from": start, "to": end})
    assert response.status_code == 422
    assert isinstance(response.json()["detail"], list)


def test_31_days_and_offsets(client):
    response = client.get(PATH, params={"from": "2026-09-19T01:00:00+01:00", "to": "2026-10-20T01:00:00+01:00"})
    assert response.status_code == 200
    assert response.json()["from"] == "2026-09-19T00:00:00Z"


def test_unknown_device_and_auth(client):
    assert client.get(f"/api/devices/{uuid4()}/history").status_code == 404
    assert client.get("/api/devices/invalid/history").status_code == 422
    client.cookies.clear()
    assert client.get(PATH).status_code == 401


def test_missing_tariff_is_not_silently_priced_at_current_rate():
    with pytest.raises(ValueError, match="No tariff"):
        report_totals(demo_history(DEVICE_ID), [])


def test_battery_zero_energy_and_variable_intervals():
    reports = demo_history(DEVICE_ID)
    reports[0].on_battery = True
    reports[0].energy_uj = 0
    reports[0].interval_seconds = 23
    totals = report_totals(reports, demo_tariffs())
    assert totals.active_time_seconds == 23
    assert totals.idle_time_seconds == 120
    assert totals.unused_time_seconds == 300
    assert totals.energy_uj == 3_900_000_000
