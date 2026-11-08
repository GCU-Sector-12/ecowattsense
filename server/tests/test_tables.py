# server/tests/test_tables.py
"""The tables of EWS_API-15: created at start, filled by the seed, rules kept, and readable by the
dashboard code."""
from datetime import date, datetime, timezone
from uuid import uuid4

import pytest
from sqlalchemy.exc import IntegrityError
from sqlmodel import Session, SQLModel, create_engine, inspect, select

from ecowattsense_server import db
from ecowattsense_server.dashboard_store import devices_from_database, history_from_database, summary_from_database
from ecowattsense_server.models import Device, EnergyRate, Location, Report, Setting, Tariff
from ecowattsense_server.seed import seed_defaults

NOW = datetime(2026, 11, 2, 12, 0, tzinfo=timezone.utc)
DEFAULT_SETTINGS = {
    "report_interval_seconds": "300",
    "ping_interval_seconds": "60",
    "idle_threshold_seconds": "600",
    "unused_threshold_seconds": "3600",
    "no_data_after_cycles": "3",
}


@pytest.fixture
def engine(tmp_path):
    """A temporary SQLite file with every table, never the real database."""
    engine = create_engine(f"sqlite:///{tmp_path / 'test.db'}", connect_args={"check_same_thread": False})
    SQLModel.metadata.create_all(engine)
    yield engine
    engine.dispose()


def seeded(engine):
    """Run the seed once."""
    with Session(engine) as session:
        seed_defaults(session)


def rows(engine, model):
    """Every row of one table."""
    with Session(engine) as session:
        return session.exec(select(model)).all()


def test_init_db_creates_every_table(tmp_path, monkeypatch):
    engine = create_engine(f"sqlite:///{tmp_path / 'start.db'}", connect_args={"check_same_thread": False})
    monkeypatch.setattr(db, "engine", engine)
    db.init_db()
    assert set(inspect(engine).get_table_names()) == {
        "devices", "reports", "locations", "tariffs", "energy_rates", "settings",
    }
    engine.dispose()


def test_seed_adds_the_first_tariff_the_watts_and_the_settings(engine):
    seeded(engine)

    (tariff,) = rows(engine, Tariff)
    assert tariff.valid_from == datetime(2000, 1, 1, tzinfo=timezone.utc)
    assert (tariff.price_gbp_per_kwh, tariff.co2_kg_per_kwh) == (0.25, 0.207)

    rates = {rate.device_type: rate for rate in rows(engine, EnergyRate)}
    assert set(rates) == {"desktop", "laptop"}
    assert all(rate.active_watts > rate.idle_watts > rate.unused_watts > 0 for rate in rates.values())
    assert [(r.active_watts, r.idle_watts, r.unused_watts) for r in (rates["desktop"], rates["laptop"])] == [(55, 40, 25), (15, 9, 3)]

    assert {setting.key: setting.value for setting in rows(engine, Setting)} == DEFAULT_SETTINGS


def test_seed_twice_adds_no_second_row(engine):
    seeded(engine)
    seeded(engine)
    assert len(rows(engine, Tariff)) == 1
    assert len(rows(engine, EnergyRate)) == 2
    assert len(rows(engine, Setting)) == 5


def test_seed_does_not_overwrite_an_edited_value(engine):
    seeded(engine)
    with Session(engine) as session:
        interval = session.exec(select(Setting).where(Setting.key == "report_interval_seconds")).one()
        interval.value = "120"
        desktop = session.exec(select(EnergyRate).where(EnergyRate.device_type == "desktop")).one()
        desktop.active_watts = 150
        session.add_all([interval, desktop])
        session.commit()

    seeded(engine)

    settings = {setting.key: setting.value for setting in rows(engine, Setting)}
    assert settings["report_interval_seconds"] == "120"
    watts = {rate.device_type: rate.active_watts for rate in rows(engine, EnergyRate)}
    assert watts["desktop"] == 150


def test_seed_adds_back_a_row_that_is_missing(engine):
    seeded(engine)
    with Session(engine) as session:
        session.delete(session.exec(select(EnergyRate).where(EnergyRate.device_type == "laptop")).one())
        session.commit()

    seeded(engine)

    assert {rate.device_type for rate in rows(engine, EnergyRate)} == {"desktop", "laptop"}


# One wrong row at a time. The database must refuse each of them (brief 5.1 and 5.4 to 5.6).
JAN_2001 = datetime(2001, 1, 1, tzinfo=timezone.utc)
WRONG_ROWS = {
    "negative active_watts": lambda: EnergyRate(device_type="a", active_watts=-1, idle_watts=1, unused_watts=1),
    "negative idle_watts": lambda: EnergyRate(device_type="a", active_watts=1, idle_watts=-1, unused_watts=1),
    "negative unused_watts": lambda: EnergyRate(device_type="a", active_watts=1, idle_watts=1, unused_watts=-1),
    "negative price": lambda: Tariff(valid_from=JAN_2001, price_gbp_per_kwh=-0.1, co2_kg_per_kwh=0.2),
    "negative co2": lambda: Tariff(valid_from=JAN_2001, price_gbp_per_kwh=0.1, co2_kg_per_kwh=-0.2),
    "a room without a name": lambda: Location(name=None),
    "a setting without a value": lambda: Setting(key="a", value=None),
}


def refuse(session, row):
    """Add one row and expect the database to refuse it."""
    session.add(row)
    with pytest.raises(IntegrityError):
        session.commit()


@pytest.mark.parametrize("make_row", WRONG_ROWS.values(), ids=WRONG_ROWS.keys())
def test_a_wrong_row_is_refused(engine, make_row):
    with Session(engine) as session:
        refuse(session, make_row())


# The same value twice in a column that must be unique.
DUPLICATES = {
    "setting key": (lambda: Setting(key="a", value="1"), lambda: Setting(key="a", value="2")),
    "tariff valid_from": (
        lambda: Tariff(valid_from=JAN_2001, price_gbp_per_kwh=0.1, co2_kg_per_kwh=0.1),
        lambda: Tariff(valid_from=JAN_2001, price_gbp_per_kwh=0.2, co2_kg_per_kwh=0.2),
    ),
    "energy_rates device_type": (
        lambda: EnergyRate(device_type="a", active_watts=1, idle_watts=1, unused_watts=1),
        lambda: EnergyRate(device_type="a", active_watts=2, idle_watts=2, unused_watts=2),
    ),
    "location name": (lambda: Location(name="Lab 1"), lambda: Location(name="Lab 1", building="B")),
}


@pytest.mark.parametrize("first,second", DUPLICATES.values(), ids=DUPLICATES.keys())
def test_the_same_value_twice_is_refused(engine, first, second):
    with Session(engine) as session:
        session.add(first())
        session.commit()
    with Session(engine) as session:
        refuse(session, second())


def test_the_dashboard_code_reads_the_real_tables(engine):
    """With the real tables and the seed, the three dashboard reads work and use the seeded tariff."""
    seeded(engine)
    device_id = uuid4()
    with Session(engine) as session:
        session.add(Location(id=1, name="Lab 1", building="George Moore"))
        session.add(Device(
            id=device_id, display_name="PC 1", location_id=1, os="windows", device_type="desktop",
            first_seen_at=NOW, last_seen_at=NOW,
        ))
        session.add(Report(
            device_id=device_id, timestamp=NOW.replace(hour=11, minute=59), received_at=NOW,
            interval_seconds=300, state=2, idle_seconds=0, cpu_percent=10.0, screen_locked=False,
            on_battery=False, energy_uj=3_600_000_000_000, energy_source="measured",
        ))
        session.commit()

    with Session(engine) as session:
        (device,) = devices_from_database(session, NOW)
        assert (device.display_name, device.location_name, device.state_name) == ("PC 1", "Lab 1", "active")

        start, end = datetime(2026, 11, 2, tzinfo=timezone.utc), datetime(2026, 11, 3, tzinfo=timezone.utc)
        history = history_from_database(session, device_id, start, end)
        assert len(history.reports) == 1
        assert history.totals.energy_uj == 3_600_000_000_000
        assert history.totals.cost_gbp == pytest.approx(0.25)
        assert history.totals.co2_kg == pytest.approx(0.207)

        summary = summary_from_database(session, date(2026, 11, 2), NOW)
        assert summary.counts.active == 1
        assert summary.price_gbp_per_kwh == 0.25
