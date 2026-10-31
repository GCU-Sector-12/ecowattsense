# ecowattsense_server/seed.py
"""Default rows for a new database."""
from sqlmodel import Session


def seed_defaults(session: Session) -> None:
    # Jason fills this in W7 (energy_rates, the first tariff, settings).
    pass
