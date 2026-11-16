# ecowattsense_server/db.py
from sqlalchemy import inspect, text
from sqlmodel import SQLModel, Session, create_engine

from .config import ROOT, settings
from .seed import seed_defaults

engine = create_engine(settings.database_url, connect_args={"check_same_thread": False})


def add_name_source_column() -> None:
    """Upgrade a devices table that was made before the column name_source existed.

    create_all does not add columns to a table that is already there, so this step does it.
    Every name that is stored then was set by a client, because only clients could set a
    name before PR 86. It is safe to run again: when the column is there, it does nothing.
    """
    columns = [column["name"] for column in inspect(engine).get_columns("devices")]
    if "name_source" in columns:
        return
    with engine.begin() as connection:
        connection.execute(text("ALTER TABLE devices ADD COLUMN name_source TEXT"))
        connection.execute(text("UPDATE devices SET name_source = 'client' WHERE display_name IS NOT NULL"))


def init_db() -> None:
    (ROOT / "data").mkdir(exist_ok=True)
    SQLModel.metadata.create_all(engine)
    add_name_source_column()
    with Session(engine) as session:
        seed_defaults(session)


def get_session():
    with Session(engine) as session:
        yield session