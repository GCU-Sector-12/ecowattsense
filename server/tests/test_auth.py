import pytest
from fastapi import Depends, FastAPI
from fastapi.testclient import TestClient

from ecowattsense_server.auth import require_client_key
from ecowattsense_server.config import Settings, settings


@pytest.fixture
def client(monkeypatch):
    monkeypatch.setattr(settings, "client_key", "test-client-key")
    app = FastAPI()

    @app.get("/protected", dependencies=[Depends(require_client_key)])
    def protected():
        return {"ok": True}

    with TestClient(app) as test_client:
        yield test_client


@pytest.mark.parametrize("headers", [{}, {"X-EWS-Key": "wrong"}, {"X-EWS-Key": ""}])
def test_missing_or_wrong_key(client, headers):
    response = client.get("/protected", headers=headers)
    assert response.status_code == 401
    assert isinstance(response.json()["detail"], str)


def test_correct_key(client):
    assert client.get("/protected", headers={"X-EWS-Key": "test-client-key"}).json() == {"ok": True}


def test_unconfigured_key_never_allows_access(client, monkeypatch):
    monkeypatch.setattr(settings, "client_key", "")
    assert client.get("/protected", headers={"X-EWS-Key": ""}).status_code == 401


def test_agreed_environment_name(monkeypatch):
    monkeypatch.setenv("EWS_CLIENT_KEY", "new-test-key")
    monkeypatch.setenv("ECOWATTSENSE_API_KEY", "obsolete-test-key")
    monkeypatch.setenv("ECOWATTSENSE_DB_PATH", "./data/test.db")
    configured = Settings(_env_file=None)
    assert configured.client_key == "new-test-key"
    assert configured.db_path == "./data/test.db"
    assert "new-test-key" not in repr(configured)
