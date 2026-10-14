# ecowattsense_server/db.py
from sqlmodel import SQLModel, Session, create_engine

from .config import ROOT, settings

engine = create_engine(settings.database_url, connect_args={"check_same_thread": False})


def init_db() -> None:
    (ROOT / "data").mkdir(exist_ok=True)
    SQLModel.metadata.create_all(engine)


def get_session():
    with Session(engine) as session:
        yield session