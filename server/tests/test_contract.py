# server/tests/test_contract.py
import pytest
from fastapi.testclient import TestClient

from ecowattsense_server.config import settings
from ecowattsense_server.main import app

client = TestClient(app)

CONTRACT_YAML = """\
openapi: 3.0.3
info:
  title: Test contract
  version: "0.0.1"
paths:
  /api/health:
    get:
      summary: Health check
      responses:
        200:
          description: The server is up.
  /api/reports:
    post:
      summary: Send reports
      responses:
        201:
          description: Created.
"""


@pytest.fixture
def contract_file(tmp_path, monkeypatch):
    """A small contract file, and the server pointed at it."""
    path = tmp_path / "openapi-contract.yaml"
    path.write_text(CONTRACT_YAML, encoding="utf-8", newline="\n")
    monkeypatch.setattr(settings, "contract_path", str(path))
    return path


@pytest.fixture
def no_contract_file(tmp_path, monkeypatch):
    """The server pointed at a contract file that does not exist."""
    monkeypatch.setattr(settings, "contract_path", str(tmp_path / "missing.yaml"))


def test_contract_page_is_swagger_ui():
    response = client.get("/api/contract")
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/html")
    assert "swagger" in response.text.lower()
    assert "/api/contract/openapi.json" in response.text


def test_contract_json_is_the_parsed_file(contract_file):
    response = client.get("/api/contract/openapi.json")
    assert response.status_code == 200
    body = response.json()
    assert body["openapi"] == "3.0.3"
    assert list(body["paths"]) == ["/api/health", "/api/reports"]
    # YAML reads the key 200 as a number, JSON needs it as text.
    health_get = body["paths"]["/api/health"]["get"]
    assert health_get["responses"]["200"]["description"] == "The server is up."


def test_contract_yaml_is_the_raw_file(contract_file):
    response = client.get("/api/contract/openapi.yaml")
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("application/yaml")
    assert response.text == CONTRACT_YAML


@pytest.mark.parametrize("url", ["/api/contract/openapi.json", "/api/contract/openapi.yaml"])
def test_contract_missing_file_is_404(no_contract_file, url):
    response = client.get(url)
    assert response.status_code == 404
    assert response.json() == {"detail": "Contract file not found"}


@pytest.mark.parametrize("url", ["/api/docs", "/api/redoc"])
def test_own_docs_pages(url):
    response = client.get(url)
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/html")


def test_own_openapi_leaves_out_the_contract_routes():
    response = client.get("/api/openapi.json")
    assert response.status_code == 200
    paths = response.json()["paths"]
    assert "/api/health" in paths
    assert "/api/contract" not in paths
    assert "/api/contract/openapi.json" not in paths
    assert "/api/contract/openapi.yaml" not in paths
