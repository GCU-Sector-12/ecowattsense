import pytest
from fastapi import Depends, FastAPI
from fastapi.testclient import TestClient

from ecowattsense_server import sessions
from ecowattsense_server.api.auth import router
from ecowattsense_server.auth import require_admin
from ecowattsense_server.config import Settings, settings


@pytest.fixture
def client(monkeypatch):
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    monkeypatch.setattr(sessions, "_sessions", {})
    app = FastAPI()
    app.include_router(router)

    @app.get("/protected", dependencies=[Depends(require_admin)])
    def protected():
        return {"ok": True}

    with TestClient(app) as test_client:
        yield test_client


def sign_in(client):
    return client.post("/api/login", json={"username": "admin", "password": "test-password"})


def test_login_logout_and_revoked_cookie(client):
    assert client.get("/protected").status_code == 401
    response = sign_in(client)
    assert response.status_code == 200
    assert response.json() == {"username": "admin"}
    cookie = response.headers["set-cookie"]
    assert "HttpOnly" in cookie and "SameSite=lax" in cookie and "Path=/" in cookie
    assert "Secure" not in cookie and "test-password" not in cookie
    token = client.cookies["ews_session"]
    assert client.get("/protected").status_code == 200
    response = client.post("/api/logout")
    assert response.status_code == 204 and response.content == b""
    assert "Max-Age=0" in response.headers["set-cookie"]
    assert client.post("/api/logout").status_code == 401
    client.cookies.set("ews_session", token)
    assert client.get("/protected").status_code == 401


@pytest.mark.parametrize("body", [
    {"username": "wrong", "password": "test-password"},
    {"username": "admin", "password": "wrong"},
    {"username": "", "password": ""},
])
def test_wrong_credentials(client, body):
    response = client.post("/api/login", json=body)
    assert response.status_code == 401
    assert response.json() == {"detail": "Wrong username or password"}
    assert "set-cookie" not in response.headers


@pytest.mark.parametrize("body", [{}, {"username": "admin"}, {"password": "pw"},
    {"username": "admin", "password": "pw", "extra": True}])
def test_invalid_body(client, body):
    assert client.post("/api/login", json=body).status_code == 422


def test_unconfigured_credentials(client, monkeypatch):
    monkeypatch.setattr(settings, "admin_password", "")
    assert sign_in(client).status_code == 401


def test_expired_and_forged_sessions(client, monkeypatch):
    client.cookies.set("ews_session", "forged")
    assert client.get("/protected").status_code == 401
    client.cookies.clear()
    monkeypatch.setattr(sessions, "monotonic", lambda: 100)
    sign_in(client)
    monkeypatch.setattr(sessions, "monotonic", lambda: 100 + settings.session_lifetime_seconds)
    assert client.get("/protected").status_code == 401


def test_login_rotates_session(client):
    sign_in(client)
    old_token = client.cookies["ews_session"]
    sign_in(client)
    assert client.cookies["ews_session"] != old_token
    assert sessions.session_user(old_token) is None


def test_https_cookie(client):
    client.base_url = "https://testserver"
    assert "Secure" in sign_in(client).headers["set-cookie"]
    assert client.get("/protected").status_code == 200
    assert "Secure" in client.post("/api/logout").headers["set-cookie"]


def test_environment_names(monkeypatch):
    monkeypatch.setenv("EWS_ADMIN_USERNAME", "admin")
    monkeypatch.setenv("EWS_ADMIN_PASSWORD", "secret-value")
    configured = Settings(_env_file=None)
    assert configured.admin_username == "admin"
    assert configured.admin_password == "secret-value"
    assert "secret-value" not in repr(configured)
