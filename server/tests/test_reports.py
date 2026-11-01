# server/tests/test_reports.py
import json
from datetime import datetime, timedelta, timezone
from pathlib import Path

import pytest
from fastapi.testclient import TestClient
from sqlmodel import Session, SQLModel, create_engine, select

from ecowattsense_server.api import reports
from ecowattsense_server.config import settings
from ecowattsense_server.db import get_session
from ecowattsense_server.main import app
from ecowattsense_server.models import Device, Report

KEY = "test-client-key"

client = TestClient(app, headers={"X-EWS-Key": KEY})
no_key_client = TestClient(app)


@pytest.fixture(autouse=True)
def client_key(monkeypatch):
    monkeypatch.setattr(settings, "client_key", KEY)


@pytest.fixture(autouse=True)
def engine(tmp_path):
    """Every test gets its own temporary SQLite file, never the real database."""
    engine = create_engine(f"sqlite:///{tmp_path / 'test.db'}", connect_args={"check_same_thread": False})
    SQLModel.metadata.create_all(engine)

    def session_for_test():
        with Session(engine) as session:
            yield session

    app.dependency_overrides[get_session] = session_for_test
    yield engine
    app.dependency_overrides.pop(get_session, None)


def stored(engine, model):
    """Every row of one table."""
    with Session(engine) as session:
        return session.exec(select(model)).all()


EXAMPLES = Path(__file__).resolve().parents[2] / "shared" / "examples"


def example(name):
    return json.loads((EXAMPLES / f"{name}.json").read_text(encoding="utf-8"))


def wrong(**changes):
    """The active example with some fields changed."""
    return example("active") | changes


@pytest.mark.parametrize("name", ["active", "idle", "unused"])
def test_single_report_is_created(name):
    response = client.post("/api/reports", json=example(name))
    assert response.status_code == 201
    assert response.json() == {"results": [{"index": 0, "status": "created", "error": None}]}


@pytest.mark.parametrize(
    "report",
    [wrong(state=3), wrong(colour="green"), wrong(cpu_percent=101), wrong(on_battery=True, energy_uj=5_000_000)],
    ids=["state 3", "unknown field", "cpu_percent 101", "energy on battery"],
)
def test_wrong_single_report_is_422(report):
    response = client.post("/api/reports", json=report)
    assert response.status_code == 422
    detail = response.json()["detail"]
    assert isinstance(detail, list) and detail
    assert detail[0]["loc"][0] == "body"


def test_missing_field_is_422():
    report = example("active")
    del report["on_battery"]
    response = client.post("/api/reports", json=report)
    assert response.status_code == 422
    assert response.json()["detail"][0]["loc"] == ["body", "on_battery"]


def test_list_is_200_with_one_result_for_each_report():
    response = client.post("/api/reports", json=[example("idle"), example("unused")])
    assert response.status_code == 200
    assert response.json() == {
        "results": [
            {"index": 0, "status": "created", "error": None},
            {"index": 1, "status": "created", "error": None},
        ]
    }


def test_list_with_one_wrong_report_is_still_200():
    response = client.post("/api/reports", json=[example("active"), wrong(state=3), example("idle")])
    assert response.status_code == 200
    results = response.json()["results"]
    assert [result["status"] for result in results] == ["created", "rejected", "created"]
    assert [result["index"] for result in results] == [0, 1, 2]
    assert results[0]["error"] is None
    assert "state" in results[1]["error"]


def test_list_of_one_is_200():
    response = client.post("/api/reports", json=[example("active")])
    assert response.status_code == 200
    assert len(response.json()["results"]) == 1


def test_list_of_100_is_200():
    response = client.post("/api/reports", json=[example("active")] * 100)
    assert response.status_code == 200
    assert len(response.json()["results"]) == 100


@pytest.mark.parametrize("size", [0, 101])
def test_list_with_0_or_more_than_100_reports_is_422(size):
    response = client.post("/api/reports", json=[example("active")] * size)
    assert response.status_code == 422
    assert response.json()["detail"][0]["loc"] == ["body"]


def test_body_that_is_not_json_is_422():
    response = client.post("/api/reports", content=b"not json", headers={"Content-Type": "application/json"})
    assert response.status_code == 422
    assert "detail" in response.json()


@pytest.mark.parametrize("body", [5, "report", None], ids=["number", "string", "null"])
def test_body_that_is_not_a_report_or_a_list_is_422(body):
    response = client.post("/api/reports", json=body)
    assert response.status_code == 422


def test_no_body_is_422():
    assert client.post("/api/reports").status_code == 422


def test_reply_holds_results_only():
    response = client.post("/api/reports", json=example("active"))
    assert set(response.json()) == {"results"}


def test_missing_key_is_401():
    response = no_key_client.post("/api/reports", json=example("active"))
    assert response.status_code == 401


def test_wrong_key_is_401():
    response = client.post("/api/reports", json=example("active"), headers={"X-EWS-Key": "wrong"})
    assert response.status_code == 401


def test_list_without_key_is_401():
    response = no_key_client.post("/api/reports", json=[example("active")])
    assert response.status_code == 401


def test_key_is_checked_before_the_body():
    response = no_key_client.post("/api/reports", json=wrong(state=3))
    assert response.status_code == 401


# Storing the reports (EWS_API-13). The tests below read the temporary database directly.

NOON = datetime(2026, 11, 2, 12, 0, tzinfo=timezone.utc)


@pytest.fixture
def clock(monkeypatch):
    """Set the server clock. clock.set(...) changes it."""

    class Clock:
        now = NOON

        def set(self, value):
            self.now = value

    clock = Clock()
    monkeypatch.setattr(reports, "utc_now", lambda: clock.now)
    return clock


def test_first_report_creates_the_device_and_the_report(engine, clock):
    report = example("active")
    response = client.post("/api/reports", json=report)
    assert response.status_code == 201
    assert response.json()["results"][0]["status"] == "created"

    (device,) = stored(engine, Device)
    assert str(device.id) == report["device_id"]
    assert (device.os, device.device_type) == ("windows", "desktop")
    assert device.display_name is None
    assert device.location_id is None
    assert device.archived is False
    assert device.first_seen_at == NOON
    assert device.last_seen_at == NOON

    (saved,) = stored(engine, Report)
    assert saved.device_id == device.id
    assert saved.timestamp == datetime(2026, 10, 19, 9, 5, tzinfo=timezone.utc)
    assert saved.received_at == NOON
    assert saved.interval_seconds == report["interval_seconds"]
    assert saved.state == report["state"]
    assert saved.idle_seconds == report["idle_seconds"]
    assert saved.cpu_percent == report["cpu_percent"]
    assert saved.screen_locked == report["screen_locked"]
    assert saved.on_battery == report["on_battery"]


def test_same_report_again_is_a_duplicate_and_is_stored_once(engine):
    assert client.post("/api/reports", json=example("active")).status_code == 201
    again = client.post("/api/reports", json=example("active"))
    assert again.status_code == 200
    assert again.json() == {"results": [{"index": 0, "status": "duplicate", "error": None}]}
    assert len(stored(engine, Report)) == 1
    assert len(stored(engine, Device)) == 1


def test_list_with_a_new_and_an_old_report_gives_created_and_duplicate_in_order(engine):
    client.post("/api/reports", json=example("idle"))
    response = client.post("/api/reports", json=[example("unused"), example("idle")])
    assert response.status_code == 200
    assert [(r["index"], r["status"]) for r in response.json()["results"]] == [(0, "created"), (1, "duplicate")]
    assert len(stored(engine, Report)) == 2
    assert len(stored(engine, Device)) == 1


def test_the_same_report_twice_in_one_list_is_stored_once(engine):
    response = client.post("/api/reports", json=[example("active"), example("active")])
    assert [r["status"] for r in response.json()["results"]] == ["created", "duplicate"]
    assert len(stored(engine, Report)) == 1


def test_last_seen_at_moves_with_every_accepted_report_and_first_seen_at_stays(engine, clock):
    client.post("/api/reports", json=example("idle"))
    clock.set(NOON + timedelta(minutes=5))
    client.post("/api/reports", json=example("unused"))
    (device,) = stored(engine, Device)
    assert device.first_seen_at == NOON
    assert device.last_seen_at == NOON + timedelta(minutes=5)


def test_a_duplicate_does_not_move_last_seen_at(engine, clock):
    client.post("/api/reports", json=example("idle"))
    clock.set(NOON + timedelta(minutes=5))
    client.post("/api/reports", json=example("idle"))
    assert stored(engine, Device)[0].last_seen_at == NOON


def test_a_late_report_is_still_stored(engine):
    client.post("/api/reports", json=example("unused"))  # 16:35
    response = client.post("/api/reports", json=example("idle"))  # 14:10, older
    assert response.json()["results"][0]["status"] == "created"
    assert len(stored(engine, Report)) == 2


def test_received_at_is_the_server_time(engine, clock):
    clock.set(datetime(2026, 11, 3, 8, 30, tzinfo=timezone.utc))
    client.post("/api/reports", json=example("active"))
    assert stored(engine, Report)[0].received_at == datetime(2026, 11, 3, 8, 30, tzinfo=timezone.utc)


def test_a_timestamp_with_an_offset_is_stored_in_utc(engine):
    client.post("/api/reports", json=wrong(timestamp="2026-10-19T10:05:00+01:00"))
    assert stored(engine, Report)[0].timestamp == datetime(2026, 10, 19, 9, 5, tzinfo=timezone.utc)
    # The same moment written in UTC is the same report.
    again = client.post("/api/reports", json=wrong(timestamp="2026-10-19T09:05:00Z"))
    assert again.json()["results"][0]["status"] == "duplicate"


def test_a_sent_energy_is_stored_as_measured(engine):
    client.post("/api/reports", json=wrong(energy_uj=9_000_000_000))
    saved = stored(engine, Report)[0]
    assert (saved.energy_uj, saved.energy_source) == (9_000_000_000, "measured")


def test_a_sent_energy_of_zero_is_stored_as_measured(engine):
    client.post("/api/reports", json=wrong(energy_uj=0))
    saved = stored(engine, Report)[0]
    assert (saved.energy_uj, saved.energy_source) == (0, "measured")


def test_a_laptop_on_battery_without_energy_is_stored_as_zero_estimated(engine):
    client.post("/api/reports", json=wrong(device_type="laptop", on_battery=True))
    saved = stored(engine, Report)[0]
    assert (saved.energy_uj, saved.energy_source) == (0, "estimated")


def test_a_report_without_energy_on_the_grid_is_zero_estimated_until_the_w7_task(engine):
    client.post("/api/reports", json=example("active"))
    saved = stored(engine, Report)[0]
    assert (saved.energy_uj, saved.energy_source) == (0, "estimated")


def test_battery_and_device_energy_are_stored(engine):
    client.post("/api/reports", json=example("idle"))
    saved = stored(engine, Report)[0]
    assert saved.battery_energy_uj == -2_700_000_000
    assert saved.device_energy_uj == 2_700_000_000


def test_machine_hash_is_kept_from_the_first_report_that_carries_it(engine):
    client.post("/api/reports", json=wrong(timestamp="2026-10-19T09:05:00Z"))
    assert stored(engine, Device)[0].machine_hash is None
    client.post("/api/reports", json=wrong(timestamp="2026-10-19T09:10:00Z", machine_hash="a" * 64))
    assert stored(engine, Device)[0].machine_hash == "a" * 64
    client.post("/api/reports", json=wrong(timestamp="2026-10-19T09:15:00Z", machine_hash="b" * 64))
    assert stored(engine, Device)[0].machine_hash == "a" * 64


def test_a_report_never_changes_the_name_or_the_device_fields(engine):
    client.post("/api/reports", json=wrong(timestamp="2026-10-19T09:05:00Z"))
    with Session(engine) as session:
        device = session.exec(select(Device)).one()
        device.display_name = "Lab 1 PC 03"
        session.add(device)
        session.commit()
    client.post("/api/reports", json=wrong(timestamp="2026-10-19T09:10:00Z", os="linux", device_type="laptop"))
    (device,) = stored(engine, Device)
    assert (device.display_name, device.os, device.device_type) == ("Lab 1 PC 03", "windows", "desktop")


def test_a_wrong_single_report_stores_nothing(engine):
    assert client.post("/api/reports", json=wrong(state=3)).status_code == 422
    assert stored(engine, Device) == []
    assert stored(engine, Report) == []


def test_a_wrong_report_in_a_list_does_not_stop_the_others(engine):
    response = client.post("/api/reports", json=[example("idle"), wrong(state=3), example("unused")])
    assert [r["status"] for r in response.json()["results"]] == ["created", "rejected", "created"]
    assert len(stored(engine, Report)) == 2


def test_reports_of_a_list_from_different_computers_make_different_devices(engine):
    client.post("/api/reports", json=[example("active"), example("idle")])
    assert len(stored(engine, Device)) == 2


def test_a_report_without_the_key_stores_nothing(engine):
    assert no_key_client.post("/api/reports", json=example("active")).status_code == 401
    assert stored(engine, Report) == []
