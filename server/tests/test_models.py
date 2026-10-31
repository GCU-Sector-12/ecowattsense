# server/tests/test_models.py
from datetime import datetime, timezone
from uuid import uuid4

import pytest
from sqlalchemy.exc import IntegrityError
from sqlmodel import Session, SQLModel, create_engine, inspect, select

from ecowattsense_server.models import Device, Report

NOW = datetime(2026, 10, 19, 14, 5, tzinfo=timezone.utc)


@pytest.fixture
def engine(tmp_path):
    """A temporary SQLite file, never the real database."""
    engine = create_engine(f"sqlite:///{tmp_path / 'test.db'}")
    SQLModel.metadata.create_all(engine)
    return engine


def a_device(**changes):
    values = dict(id=uuid4(), os="windows", device_type="laptop", first_seen_at=NOW, last_seen_at=NOW)
    return Device(**(values | changes))


def a_report(device_id, **changes):
    values = dict(
        device_id=device_id, timestamp=NOW, received_at=NOW, interval_seconds=300, state=2,
        idle_seconds=12, cpu_percent=37.5, screen_locked=False, on_battery=False,
        energy_uj=9_000_000_000, energy_source="estimated",
    )
    return Report(**(values | changes))


def test_create_all_makes_both_tables(engine):
    assert {"devices", "reports"} <= set(inspect(engine).get_table_names())


def test_columns_match_the_brief(engine):
    columns = lambda table: [column["name"] for column in inspect(engine).get_columns(table)]
    assert columns("devices") == [
        "id", "display_name", "location_id", "machine_hash", "os", "device_type",
        "first_seen_at", "last_seen_at", "archived",
    ]
    assert columns("reports") == [
        "id", "device_id", "timestamp", "received_at", "interval_seconds", "state",
        "idle_seconds", "cpu_percent", "screen_locked", "on_battery", "energy_uj",
        "energy_source", "battery_energy_uj", "device_energy_uj",
    ]


def test_device_can_be_saved_and_read_back(engine):
    device = a_device()
    device_id = device.id  # take it before the commit: a saved object expires
    with Session(engine) as session:
        session.add(device)
        session.commit()
    with Session(engine) as session:
        saved = session.get(Device, device_id)
        assert saved.os == "windows"
        assert saved.device_type == "laptop"
        assert saved.archived is False
        assert saved.display_name is None
        assert saved.location_id is None
        assert saved.machine_hash is None


def test_report_can_be_saved_and_read_back(engine):
    device = a_device()
    device_id = device.id
    with Session(engine) as session:
        session.add(device)
        session.add(a_report(device_id))
        session.commit()
    with Session(engine) as session:
        saved = session.exec(select(Report)).one()
        assert saved.id is not None
        assert saved.device_id == device_id
        assert saved.state == 2
        assert saved.cpu_percent == 37.5
        assert saved.energy_uj == 9_000_000_000
        assert saved.battery_energy_uj is None
        assert saved.device_energy_uj is None


def test_report_energy_can_be_bigger_than_32_bits_and_battery_can_be_negative(engine):
    device = a_device()
    with Session(engine) as session:
        session.add(device)
        session.add(a_report(device.id, battery_energy_uj=-2_700_000_000, device_energy_uj=2_700_000_000))
        session.commit()
    with Session(engine) as session:
        saved = session.exec(select(Report)).one()
        assert saved.battery_energy_uj == -2_700_000_000
        assert saved.device_energy_uj == 2_700_000_000


def test_same_device_and_timestamp_is_rejected(engine):
    device = a_device()
    device_id = device.id
    with Session(engine) as session:
        session.add(device)
        session.add(a_report(device_id))
        session.commit()
    with Session(engine) as session:
        session.add(a_report(device_id))
        with pytest.raises(IntegrityError):
            session.commit()


def test_same_timestamp_for_another_device_is_fine(engine):
    first, second = a_device(), a_device()
    with Session(engine) as session:
        session.add_all([first, second])
        session.add(a_report(first.id))
        session.add(a_report(second.id))
        session.commit()
        assert len(session.exec(select(Report)).all()) == 2


def test_timestamp_has_an_index(engine):
    indexed = [index["column_names"] for index in inspect(engine).get_indexes("reports")]
    assert ["timestamp"] in indexed
