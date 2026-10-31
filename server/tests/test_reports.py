# server/tests/test_reports.py
import json
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from ecowattsense_server.config import settings
from ecowattsense_server.main import app

KEY = "test-client-key"

client = TestClient(app, headers={"X-EWS-Key": KEY})
no_key_client = TestClient(app)


@pytest.fixture(autouse=True)
def client_key(monkeypatch):
    monkeypatch.setattr(settings, "client_key", KEY)

EXAMPLES = Path(__file__).resolve().parents[2] / "shared" / "examples"


def example(name):
    return json.loads((EXAMPLES / f"{name}.json").read_text(encoding="utf-8"))


def wrong(**changes):
    """The active example with some fields changed."""
    return example("active") | changes


@pytest.mark.parametrize("name", ["active", "idle", "unused"])
def test_single_report_is_created(name):
    response = client.post("/api/reports", json=example(name))
    assert response.status_code == 201
    assert response.json() == {"results": [{"index": 0, "status": "created", "error": None}]}


@pytest.mark.parametrize(
    "report",
    [wrong(state=3), wrong(colour="green"), wrong(cpu_percent=101), wrong(on_battery=True, energy_uj=5_000_000)],
    ids=["state 3", "unknown field", "cpu_percent 101", "energy on battery"],
)
def test_wrong_single_report_is_422(report):
    response = client.post("/api/reports", json=report)
    assert response.status_code == 422
    detail = response.json()["detail"]
    assert isinstance(detail, list) and detail
    assert detail[0]["loc"][0] == "body"


def test_missing_field_is_422():
    report = example("active")
    del report["on_battery"]
    response = client.post("/api/reports", json=report)
    assert response.status_code == 422
    assert response.json()["detail"][0]["loc"] == ["body", "on_battery"]


def test_list_is_200_with_one_result_for_each_report():
    response = client.post("/api/reports", json=[example("idle"), example("unused")])
    assert response.status_code == 200
    assert response.json() == {
        "results": [
            {"index": 0, "status": "created", "error": None},
            {"index": 1, "status": "created", "error": None},
        ]
    }


def test_list_with_one_wrong_report_is_still_200():
    response = client.post("/api/reports", json=[example("active"), wrong(state=3), example("idle")])
    assert response.status_code == 200
    results = response.json()["results"]
    assert [result["status"] for result in results] == ["created", "rejected", "created"]
    assert [result["index"] for result in results] == [0, 1, 2]
    assert results[0]["error"] is None
    assert "state" in results[1]["error"]


def test_list_of_one_is_200():
    response = client.post("/api/reports", json=[example("active")])
    assert response.status_code == 200
    assert len(response.json()["results"]) == 1


def test_list_of_100_is_200():
    response = client.post("/api/reports", json=[example("active")] * 100)
    assert response.status_code == 200
    assert len(response.json()["results"]) == 100


@pytest.mark.parametrize("size", [0, 101])
def test_list_with_0_or_more_than_100_reports_is_422(size):
    response = client.post("/api/reports", json=[example("active")] * size)
    assert response.status_code == 422
    assert response.json()["detail"][0]["loc"] == ["body"]


def test_body_that_is_not_json_is_422():
    response = client.post("/api/reports", content=b"not json", headers={"Content-Type": "application/json"})
    assert response.status_code == 422
    assert "detail" in response.json()


@pytest.mark.parametrize("body", [5, "report", None], ids=["number", "string", "null"])
def test_body_that_is_not_a_report_or_a_list_is_422(body):
    response = client.post("/api/reports", json=body)
    assert response.status_code == 422


def test_no_body_is_422():
    assert client.post("/api/reports").status_code == 422


def test_reply_holds_results_only():
    response = client.post("/api/reports", json=example("active"))
    assert set(response.json()) == {"results"}


def test_missing_key_is_401():
    response = no_key_client.post("/api/reports", json=example("active"))
    assert response.status_code == 401


def test_wrong_key_is_401():
    response = client.post("/api/reports", json=example("active"), headers={"X-EWS-Key": "wrong"})
    assert response.status_code == 401


def test_list_without_key_is_401():
    response = no_key_client.post("/api/reports", json=[example("active")])
    assert response.status_code == 401


def test_key_is_checked_before_the_body():
    response = no_key_client.post("/api/reports", json=wrong(state=3))
    assert response.status_code == 401
