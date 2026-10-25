"""Integration against temporary SQLite tables matching the agreed data brief.

These tests do not create or modify the application's pending models.py.
"""
from datetime import date, datetime, timedelta, timezone
from uuid import UUID

import pytest
from fastapi import HTTPException
from fastapi.testclient import TestClient
from sqlalchemy import BigInteger, Boolean, Column, DateTime, Float, Integer, MetaData, String, Table, insert
from sqlmodel import Session, create_engine
from sqlalchemy.pool import StaticPool

from ecowattsense_server.config import settings
from ecowattsense_server.dashboard_store import devices_from_database, history_from_database, summary_from_database
from ecowattsense_server.db import get_session
from ecowattsense_server.main import app

DEVICE_ID = UUID("6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12")
SECOND_ID = UUID("6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a13")
THIRD_ID = UUID("6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a14")
NOW = datetime(2026, 10, 19, 14, 6, tzinfo=timezone.utc)


@pytest.fixture
def database():
    engine = create_engine("sqlite://", connect_args={"check_same_thread": False}, poolclass=StaticPool)
    meta = MetaData()
    devices = Table("devices", meta, Column("id", String(32), primary_key=True),
        Column("display_name", String), Column("location_id", Integer), Column("machine_hash", String),
        Column("os", String), Column("device_type", String), Column("first_seen_at", DateTime),
        Column("last_seen_at", DateTime), Column("archived", Boolean))
    reports = Table("reports", meta, Column("id", Integer, primary_key=True), Column("device_id", String(32)),
        Column("timestamp", DateTime), Column("received_at", DateTime), Column("interval_seconds", Integer),
        Column("state", Integer), Column("idle_seconds", Integer), Column("cpu_percent", Float),
        Column("screen_locked", Boolean), Column("on_battery", Boolean), Column("energy_uj", BigInteger),
        Column("energy_source", String), Column("battery_energy_uj", BigInteger), Column("device_energy_uj", BigInteger))
    tariffs = Table("tariffs", meta, Column("id", Integer, primary_key=True), Column("valid_from", DateTime),
        Column("price_gbp_per_kwh", Float), Column("co2_kg_per_kwh", Float))
    locations = Table("locations", meta, Column("id", Integer, primary_key=True), Column("name", String), Column("building", String))
    meta.create_all(engine)
    with Session(engine) as session:
        session.execute(insert(locations), [{"id": 1, "name": "Lab 1", "building": "George Moore"}])
        session.execute(insert(devices), [{
            "id": identity.hex, "display_name": name, "location_id": location,
            "os": "windows", "device_type": "laptop", "first_seen_at": datetime(2026, 10, 1),
            "last_seen_at": datetime(2026, 10, 19, 14, 5), "archived": archived,
        } for identity, name, location, archived in [
            (DEVICE_ID, "PC 1", 1, False), (SECOND_ID, None, None, True), (THIRD_ID, "PC 3", None, False)]])
        session.execute(insert(tariffs), [
            {"valid_from": datetime(2000, 1, 1), "price_gbp_per_kwh": .20, "co2_kg_per_kwh": .25},
            {"valid_from": datetime(2026, 10, 19, 14), "price_gbp_per_kwh": .30, "co2_kg_per_kwh": .10},
        ])
        # Latest report inserted first; an older late upload must not win.
        session.execute(insert(reports), [{
            "device_id": DEVICE_ID.hex, "timestamp": stamp, "received_at": datetime(2026, 10, 19, 14, 5),
            "interval_seconds": interval, "state": state, "idle_seconds": 0, "cpu_percent": 1.0,
            "screen_locked": False, "on_battery": False, "energy_uj": energy, "energy_source": "measured",
        } for stamp, interval, state, energy in [
            (datetime(2026, 10, 19, 14, 5), 120, 1, 3_600_000_000_000),
            (datetime(2026, 10, 19, 13, 55), 300, 2, 3_600_000_000_000),
        ]])
        session.commit()
        yield session
    engine.dispose()


def test_database_devices_and_staleness(database):
    rows = {row.device_id: row for row in devices_from_database(database, NOW)}
    assert rows[DEVICE_ID].state == 1
    assert rows[DEVICE_ID].state_name == "idle"
    assert rows[DEVICE_ID].location_name == "Lab 1"
    assert rows[THIRD_ID].state is None and rows[THIRD_ID].state_name == "no_data"
    assert rows[SECOND_ID].archived
    rows = {row.device_id: row for row in devices_from_database(database, NOW + timedelta(minutes=14))}
    assert rows[DEVICE_ID].state_name == "no_data"


def test_database_history_and_tariffs(database):
    data = history_from_database(database, DEVICE_ID, NOW - timedelta(hours=1), NOW)
    assert data.totals.energy_uj == 7_200_000_000_000
    assert data.totals.cost_gbp == pytest.approx(.50)
    assert data.totals.co2_kg == pytest.approx(.35)
    assert data.totals.active_time_seconds == 300
    assert data.totals.idle_time_seconds == 120
    assert data.reports[0].timestamp < data.reports[1].timestamp
    # The end is exclusive.
    data = history_from_database(database, DEVICE_ID, NOW - timedelta(hours=1), NOW - timedelta(minutes=1))
    assert len(data.reports) == 1
    data = history_from_database(database, SECOND_ID, NOW - timedelta(hours=1), NOW)
    assert data.reports == [] and data.totals.energy_uj == 0


def test_database_summary(database):
    data = summary_from_database(database, date(2026, 10, 19), NOW)
    assert data.counts.total == 2 and data.counts.idle == 1 and data.counts.no_data == 1
    assert data.energy_uj == 7_200_000_000_000
    assert data.cost_gbp == pytest.approx(.50)
    assert data.price_gbp_per_kwh == .30
    assert sum(point.energy_uj for point in data.series) == data.energy_uj
    assert all(point.timestamp < NOW for point in data.series)
    assert data.series[0].timestamp == datetime(2026, 10, 18, 23, tzinfo=timezone.utc)


@pytest.mark.parametrize("day,count", [(date(2026, 3, 29), 46), (date(2026, 10, 25), 50)])
def test_database_dst(database, day, count):
    data = summary_from_database(database, day, datetime(2026, 11, 1, tzinfo=timezone.utc))
    assert len(data.series) == count
    assert data.energy_uj == 0 and data.cost_gbp == 0


def test_database_future_day(database):
    data = summary_from_database(database, date(2026, 10, 20), NOW)
    assert data.series == [] and data.energy_uj == 0


def test_missing_tables_and_tariff_fail_clearly(database):
    tariff = Table("tariffs", MetaData(), autoload_with=database.connection())
    database.execute(tariff.delete())
    database.commit()
    with pytest.raises(HTTPException) as exc:
        history_from_database(database, DEVICE_ID, NOW - timedelta(hours=1), NOW)
    assert exc.value.status_code == 503
    tariff.drop(database.connection())
    with pytest.raises(HTTPException) as exc:
        summary_from_database(database, date(2026, 10, 19), NOW)
    assert exc.value.status_code == 503


def test_database_routes(database, monkeypatch):
    monkeypatch.setattr(settings, "dashboard_data_source", "database")
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    app.dependency_overrides[get_session] = lambda: database
    try:
        with TestClient(app) as client:
            assert client.get("/api/devices").status_code == 401
            assert client.post("/api/login", json={"username": "admin", "password": "test-password"}).status_code == 200
            assert len(client.get("/api/devices?include_archived=true").json()["devices"]) == 3
            assert len(client.get("/api/devices?location_id=1").json()["devices"]) == 1
            response = client.get(f"/api/devices/{DEVICE_ID}/history", params={"from": "2026-10-19T13:00:00Z", "to": "2026-10-19T15:00:00Z"})
            assert response.status_code == 200
            assert response.json()["totals"]["cost_gbp"] == pytest.approx(.5)
            assert client.get("/api/summary?date=2026-10-19").status_code == 200
            assert client.get("/api/devices/00000000-0000-0000-0000-000000000000/history").status_code == 404
    finally:
        app.dependency_overrides.pop(get_session, None)
