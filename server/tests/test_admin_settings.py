"""Admin settings API tests with isolated SQLite storage and real session authentication."""
from concurrent.futures import ThreadPoolExecutor

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import event
from sqlmodel import Session, create_engine, select

from ecowattsense_server import db, sessions
from ecowattsense_server.config import settings
from ecowattsense_server.main import app
from ecowattsense_server.models import Setting, Tariff

URL = "/api/admin/settings"


@pytest.fixture
def client(tmp_path, monkeypatch):
    """Start the application with a seeded temporary database and an admin cookie.

    Args:
        tmp_path: Pytest temporary folder.
        monkeypatch: Scoped configuration overrides.
    Returns:
        Yields a signed-in test client with its database engine.
    Raises:
        AssertionError: Authentication fails.
    """
    engine = create_engine(f"sqlite:///{tmp_path / 'settings.db'}", connect_args={"check_same_thread": False})
    monkeypatch.setattr(db, "engine", engine)
    monkeypatch.setattr(settings, "admin_username", "settings-admin")
    monkeypatch.setattr(settings, "admin_password", "settings-test-password")
    monkeypatch.setattr(settings, "client_key", "settings-client-key")
    monkeypatch.setattr(sessions, "_sessions", {})
    with TestClient(app) as test_client:
        sign_in(test_client)
        test_client.test_engine = engine
        yield test_client
    engine.dispose()


def sign_in(client):
    """Authenticate the test admin.

    Args:
        client: Local test HTTP client.
    Returns:
        None.
    Raises:
        AssertionError: The login endpoint rejects the test credentials.
    """
    response = client.post("/api/login", json={"username": "settings-admin", "password": "settings-test-password"})
    assert response.status_code == 200


def test_read_save_reload_and_client_version(client):
    """Check the normal admin save and the client's settings and ping views."""
    before = client.get(URL)
    assert before.status_code == 200
    body = before.json() | {"report_interval_seconds": 420, "idle_threshold_seconds": 900,
                            "unused_threshold_seconds": 1800, "price_gbp_per_kwh": 0.31}
    saved = client.put(URL, json=body)
    assert saved.status_code == 200
    assert saved.json() == body | {"settings_version": 2}
    assert client.get(URL).json() == saved.json()
    headers = {"X-EWS-Key": "settings-client-key"}
    assert client.get("/api/settings", headers=headers).json() == saved.json()
    ping = client.post("/api/ping", headers=headers,
                       json={"device_id": "5f0c1e7a-3b9d-4c1e-8a52-0d6e4f7b9a11", "unsent_reports": 0})
    assert ping.json()["settings_version"] == 2


@pytest.mark.parametrize("cookie", [None, "forged"])
def test_requires_admin_session_even_with_client_key(client, cookie):
    """Reject reads and writes without a valid admin session."""
    body = client.get(URL).json()
    client.cookies.clear()
    if cookie:
        client.cookies.set("ews_session", cookie)
    headers = {"X-EWS-Key": "settings-client-key"}
    assert client.get(URL, headers=headers).status_code == 401
    assert client.put(URL, headers=headers, json=body).status_code == 401


@pytest.mark.parametrize("field,value", [
    ("report_interval_seconds", 0), ("report_interval_seconds", 59), ("report_interval_seconds", 3601),
    ("report_interval_seconds", 60.5), ("report_interval_seconds", "300"), ("report_interval_seconds", True),
    ("idle_threshold_seconds", 0), ("unused_threshold_seconds", 600),
    ("ping_interval_seconds", -1), ("no_data_after_cycles", 0), ("idle_threshold_seconds", 2147483648),
    ("price_gbp_per_kwh", -1), ("price_gbp_per_kwh", "0.25"), ("price_gbp_per_kwh", True),
    ("co2_kg_per_kwh", -1), ("co2_kg_per_kwh", None),
    ("tariff_valid_from", "2026-11-15T00:00:00"), ("tariff_valid_from", "bad-date"),
    ("settings_version", 0), ("extra", 1),
])
def test_invalid_values_leave_settings_unchanged(client, field, value):
    """Return 422 for invalid values and keep the stored version and values."""
    before = client.get(URL).json()
    response = client.put(URL, json=before | {field: value})
    assert response.status_code == 422
    assert response.json()["detail"]
    assert client.get(URL).json() == before


def test_missing_and_non_json_body(client):
    """Reject incomplete and non-JSON settings saves."""
    assert client.put(URL, json={}).status_code == 422
    assert client.put(URL, content="{}", headers={"Content-Type": "text/plain"}).status_code == 422


def test_stale_retry_does_not_overwrite(client):
    """An old version cannot replace a successful save."""
    original = client.get(URL).json()
    assert client.put(URL, json=original | {"report_interval_seconds": 600}).status_code == 200
    retry = client.put(URL, json=original | {"report_interval_seconds": 900})
    assert retry.status_code == 409
    assert client.get(URL).json()["report_interval_seconds"] == 600
    assert client.get(URL).json()["settings_version"] == 2


def tariff_count(engine):
    """Read tariff count using a separate database session.

    Args:
        engine: Isolated test engine.
    Returns:
        Number of tariff periods.
    Raises:
        SQLAlchemyError: Database read fails.
    """
    with Session(engine) as session:
        return len(session.exec(select(Tariff)).all())


def test_future_tariff_and_equivalent_timezone_update(client):
    """Preserve current tariff while saving and updating a future period."""
    body = client.get(URL).json()
    future = body | {"tariff_valid_from": "2099-01-01T01:00:00+01:00", "price_gbp_per_kwh": 0.5}
    saved = client.put(URL, json=future)
    assert saved.status_code == 200
    assert saved.json()["tariff_valid_from"] == "2099-01-01T00:00:00Z"
    assert client.get(URL).json()["price_gbp_per_kwh"] == body["price_gbp_per_kwh"]
    assert client.put(URL, json=saved.json() | {"price_gbp_per_kwh": 0.6}).status_code == 200
    assert tariff_count(client.test_engine) == 2


def remove_integer_rows(engine):
    """Remove seeded setting rows to exercise old databases.

    Args:
        engine: Isolated test engine.
    Returns:
        None.
    Raises:
        SQLAlchemyError: Database write fails.
    """
    with Session(engine) as session:
        session.execute(Setting.__table__.delete())
        session.commit()


def test_old_database_without_setting_rows(client):
    """Fallback settings can be saved even without seed or version rows."""
    remove_integer_rows(client.test_engine)
    body = client.get(URL).json()
    assert body["settings_version"] == 1
    assert client.put(URL, json=body).status_code == 200
    assert client.get(URL).json()["settings_version"] == 2


def concurrent_save(body, cookie):
    """Submit a save through a separate HTTP client.

    Args:
        body: Settings request with a shared original version.
        cookie: Valid admin session token.
    Returns:
        HTTP status code of the save.
    Raises:
        Exception: The HTTP test transport fails.
    """
    peer = TestClient(app)
    peer.cookies.set("ews_session", cookie)
    return peer.put(URL, json=body).status_code


def test_concurrent_saves_allow_one_writer(client):
    """Two edits from the same version produce one success and one conflict."""
    body = client.get(URL).json()
    cookie = client.cookies.get("ews_session")
    with ThreadPoolExecutor(max_workers=2) as pool:
        first = pool.submit(concurrent_save, body | {"report_interval_seconds": 600}, cookie)
        second = pool.submit(concurrent_save, body | {"report_interval_seconds": 900}, cookie)
        statuses = [first.result(), second.result()]
    assert sorted(statuses) == [200, 409]
    assert client.get(URL).json()["settings_version"] == 2


def fail_tariff_write(conn, cursor, statement, parameters, context, executemany):
    """Raise at the tariff write to verify rollback after settings changes.

    Args:
        conn, cursor, statement, parameters, context, executemany: SQLAlchemy event arguments.
    Returns:
        None.
    Raises:
        RuntimeError: The tariff insert is reached.
    """
    if statement.startswith("INSERT INTO tariffs"):
        raise RuntimeError("Injected tariff failure")


def test_failed_tariff_write_rolls_back_settings_and_version(client):
    """A failure after settings updates leaves the whole previous state intact."""
    before = client.get(URL).json()
    event.listen(client.test_engine, "before_cursor_execute", fail_tariff_write)
    with pytest.raises(RuntimeError, match="Injected tariff failure"):
        client.put(URL, json=before | {"report_interval_seconds": 120})
    event.remove(client.test_engine, "before_cursor_execute", fail_tariff_write)
    assert client.get(URL).json() == before
