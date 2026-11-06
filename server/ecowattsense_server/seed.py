# ecowattsense_server/seed.py
"""Starting rows for a new database: the first tariff, the watts per device type and the settings.

init_db() calls seed_defaults() after the tables are created, at every start of the server. It
adds only the rows that are missing, so it can run again without a second copy of a row, and it
never changes a row that an admin has edited.
"""
from datetime import datetime, timezone

from sqlmodel import Session, select

from .models import EnergyRate, Setting, Tariff

# The starting values are plain data, not table objects. A table object belongs to one session,
# so a shared one breaks the second time the seed runs. Each run builds new objects from this data.

# The first tariff starts at 2000-01-01, so it covers every report (brief 5.5).
FIRST_TARIFF = {
    "valid_from": datetime(2000, 1, 1, tzinfo=timezone.utc),
    "price_gbp_per_kwh": 0.25,
    "co2_kg_per_kwh": 0.207,
}

# Starting watts for a computer that is plugged in (brief 5.4). Source: Planning Report [1],
# Bray (2006), which summarises Kawamoto et al. (2001). Active and unused are the "active" and
# "low power" values of that study. Idle is not in the study: it is the middle value, which is
# our own assumption. The admin can change these, and a measured energy_uj from the client
# replaces the estimate.
ENERGY_RATES = [
    {"device_type": "desktop", "active_watts": 55, "idle_watts": 40, "unused_watts": 25},
    {"device_type": "laptop", "active_watts": 15, "idle_watts": 9, "unused_watts": 3},
]

# The default settings, as text (brief 5.6).
SETTINGS = {
    "report_interval_seconds": "300",
    "ping_interval_seconds": "60",
    "idle_threshold_seconds": "600",
    "unused_threshold_seconds": "3600",
    "no_data_after_cycles": "3",
}


def seed_tariff(session: Session) -> None:
    """Add the first tariff when the tariffs table has none.

    Args:
        session: The database session.
    """
    if session.exec(select(Tariff.id)).first() is None:
        session.add(Tariff(**FIRST_TARIFF))


def seed_energy_rates(session: Session) -> None:
    """Add the watts of every device type that has no row yet.

    Args:
        session: The database session.
    """
    known = set(session.exec(select(EnergyRate.device_type)).all())
    session.add_all([EnergyRate(**rate) for rate in ENERGY_RATES if rate["device_type"] not in known])


def seed_settings(session: Session) -> None:
    """Add every default setting that has no row yet. A changed value is never overwritten.

    Args:
        session: The database session.
    """
    known = set(session.exec(select(Setting.key)).all())
    session.add_all([Setting(key=key, value=value) for key, value in SETTINGS.items() if key not in known])


def seed_defaults(session: Session) -> None:
    """Fill a new database with the starting rows. Safe to run again.

    Args:
        session: The database session.
    """
    seed_tariff(session)
    seed_energy_rates(session)
    seed_settings(session)
    session.commit()
