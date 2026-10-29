from datetime import date, datetime, timedelta

import pytest
from fastapi.testclient import TestClient

from ecowattsense_server.api import summary
from ecowattsense_server.config import settings
from ecowattsense_server.main import app


@pytest.fixture
def client(monkeypatch):
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    with TestClient(app) as test_client:
        assert test_client.post("/api/login", json={"username": "admin", "password": "test-password"}).status_code == 200
        yield test_client


@pytest.mark.parametrize("day,count,first", [
    ("2026-10-19", 48, "2026-10-18T23:00:00Z"),
    ("2026-01-19", 48, "2026-01-19T00:00:00Z"),
    ("2026-03-29", 46, "2026-03-29T00:00:00Z"),
    ("2026-10-25", 50, "2026-10-24T23:00:00Z"),
])
def test_summary_day_and_totals(client, day, count, first):
    response = client.get(f"/api/summary?date={day}")
    assert response.status_code == 200
    data = response.json()
    assert set(data) == {"date", "counts", "energy_uj", "cost_gbp", "co2_kg", "price_gbp_per_kwh", "co2_kg_per_kwh", "series"}
    assert data["date"] == day
    assert len(data["series"]) == count
    assert data["series"][0]["timestamp"] == first
    timestamps = [datetime.fromisoformat(point["timestamp"]) for point in data["series"]]
    assert all(b - a == timedelta(minutes=30) for a, b in zip(timestamps, timestamps[1:]))
    assert data["energy_uj"] == sum(point["energy_uj"] for point in data["series"])
    assert data["cost_gbp"] == pytest.approx(data["energy_uj"] / 3.6e12 * .25)
    assert data["co2_kg"] == pytest.approx(data["energy_uj"] / 3.6e12 * .207)
    assert data["counts"]["total"] == sum(data["counts"][key] for key in ("active", "idle", "unused", "no_data"))


def test_default_day(client, monkeypatch):
    monkeypatch.setattr(summary, "uk_today", lambda: date(2026, 7, 1))
    assert client.get("/api/summary").json()["date"] == "2026-07-01"


@pytest.mark.parametrize("value", ["wrong", "2026-02-30", "2026-13-01", "2026-1-1", "0", "2026-10-19T00:00:00Z", "9999-12-31"])
def test_invalid_date(client, value):
    response = client.get("/api/summary", params={"date": value})
    assert response.status_code == 422
    assert isinstance(response.json()["detail"], list)


def test_cookie_required(client):
    client.cookies.clear()
    assert client.get("/api/summary").status_code == 401
    client.cookies.set("ews_session", "forged")
    assert client.get("/api/summary").status_code == 401
