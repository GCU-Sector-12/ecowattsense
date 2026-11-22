# server/tests/test_connect.py
"""The Connect endpoints C1 to C4: a computer asks for the client key and the admin approves it.

The tests use the real app but never the database, because the store of requests lives in memory.
The clock of the store is replaced by a fake one, so no test sleeps.
"""
import logging
import re

import pytest
from fastapi.testclient import TestClient

from ecowattsense_server import connect_requests
from ecowattsense_server.config import Settings, settings
from ecowattsense_server.main import app

DEVICE_ID = "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12"
CLIENT_KEY = "client-test-key-0123456789"
KEY_MISSING = {"detail": "Client key is not configured"}
NOT_FOUND = {"detail": "Connect request not found"}
CODE_PATTERN = re.compile(r"^[A-HJ-NP-Z2-9]{4}-[A-HJ-NP-Z2-9]{4}$")
GOOD_BODY = {"device_id": DEVICE_ID, "device_name": "Lab 1 PC 03", "os": "windows", "device_type": "laptop"}
UNKNOWN_ID = "x" * 43


class FakeClock:
    """A clock that only moves when a test says so."""

    def __init__(self) -> None:
        self.seconds = 1000.0

    def now(self) -> float:
        """Give the fake time in seconds."""
        return self.seconds

    def advance(self, seconds: float) -> None:
        """Move the fake time forward."""
        self.seconds += seconds


@pytest.fixture
def clock(monkeypatch) -> FakeClock:
    """An empty store, a fake clock for it and the default lifetime of 600 seconds."""
    fake = FakeClock()
    monkeypatch.setattr(connect_requests, "_requests", {})
    monkeypatch.setattr(connect_requests, "now", fake.now)
    monkeypatch.setattr(settings, "connect_request_lifetime_seconds", 600)
    monkeypatch.setattr(settings, "client_key", CLIENT_KEY)
    return fake


@pytest.fixture
def anon(clock) -> TestClient:
    """A caller with no key and no session, like Setup."""
    return TestClient(app)


@pytest.fixture
def admin(clock, monkeypatch) -> TestClient:
    """A caller with an admin session, like the dashboard."""
    monkeypatch.setattr(settings, "admin_username", "admin")
    monkeypatch.setattr(settings, "admin_password", "test-password")
    browser = TestClient(app)
    response = browser.post("/api/login", json={"username": "admin", "password": "test-password"})
    assert response.status_code == 200
    return browser


def make_request(anon: TestClient, **changes) -> dict:
    """Send C1 with the good body and the changes, and give the answer body."""
    response = anon.post("/api/connect/requests", json={**GOOD_BODY, **changes})
    assert response.status_code == 201
    return response.json()


def claim(anon: TestClient, request_id: str):
    """Send C4."""
    return anon.post("/api/connect/claim", json={"request_id": request_id})


def approve_url(user_code: str) -> str:
    """Give the address of C3."""
    return f"/api/admin/connect/requests/{user_code}/approve"


def info_url(user_code: str) -> str:
    """Give the address of C2."""
    return f"/api/admin/connect/requests/{user_code}"


# ---- C1 ----

def test_c1_good_body(anon):
    response = anon.post("/api/connect/requests", json=GOOD_BODY)
    assert response.status_code == 201
    body = response.json()
    assert set(body) == {"request_id", "user_code", "expires_in_seconds"}
    assert body["expires_in_seconds"] == 600
    assert re.fullmatch(r"[A-Za-z0-9_-]{43}", body["request_id"])
    assert CODE_PATTERN.match(body["user_code"])
    assert response.headers["cache-control"] == "no-store"


def test_c1_needs_no_key_and_no_session(anon):
    assert "x-ews-key" not in anon.headers
    assert "ews_session" not in anon.cookies
    assert anon.post("/api/connect/requests", json=GOOD_BODY).status_code == 201


@pytest.mark.parametrize("changes", [
    {"device_name": None},
    {"device_name": "   "},
    {"device_name": "x" * 60},
])
def test_c1_name_rules_are_the_ones_of_device_in(anon, admin, changes):
    made = make_request(anon, **changes)
    shown = admin.get(info_url(made["user_code"])).json()
    expected = changes["device_name"]
    assert shown["device_name"] == (expected if expected and expected.strip() else None)


def test_c1_name_may_be_left_out(anon, admin):
    body = {key: value for key, value in GOOD_BODY.items() if key != "device_name"}
    response = anon.post("/api/connect/requests", json=body)
    assert response.status_code == 201
    assert admin.get(info_url(response.json()["user_code"])).json()["device_name"] is None


@pytest.mark.parametrize("changes", [
    {"device_id": "not-a-uuid"},
    {"device_id": 7},
    {"os": "plan9"},
    {"os": None},
    {"device_type": "Laptop"},
    {"device_type": "1laptop"},
    {"device_name": "x" * 61},
    {"device_name": "bad\x07name"},
])
def test_c1_bad_body_is_422(anon, changes):
    response = anon.post("/api/connect/requests", json={**GOOD_BODY, **changes})
    assert response.status_code == 422
    assert isinstance(response.json()["detail"], list)


@pytest.mark.parametrize("missing", ["device_id", "os", "device_type"])
def test_c1_missing_field_is_422(anon, missing):
    body = {key: value for key, value in GOOD_BODY.items() if key != missing}
    assert anon.post("/api/connect/requests", json=body).status_code == 422


def test_c1_unknown_field_is_422(anon):
    response = anon.post("/api/connect/requests", json={**GOOD_BODY, "machine_hash": "abc"})
    assert response.status_code == 422


def test_c1_without_a_client_key_is_503(anon, monkeypatch):
    monkeypatch.setattr(settings, "client_key", "")
    response = anon.post("/api/connect/requests", json=GOOD_BODY)
    assert response.status_code == 503
    assert response.json() == KEY_MISSING
    assert response.headers["cache-control"] == "no-store"


def test_c1_creates_nothing_when_the_key_is_missing(anon, monkeypatch):
    monkeypatch.setattr(settings, "client_key", "")
    anon.post("/api/connect/requests", json=GOOD_BODY)
    assert connect_requests._requests == {}


def test_c1_refuses_the_21st_pending_request(anon):
    for _ in range(20):
        make_request(anon)
    response = anon.post("/api/connect/requests", json=GOOD_BODY)
    assert response.status_code == 429
    assert response.json() == {"detail": "Too many connect requests"}
    assert response.headers["cache-control"] == "no-store"


def test_c1_expired_requests_do_not_count_as_pending(anon, clock):
    for _ in range(20):
        make_request(anon)
    clock.advance(600)
    assert anon.post("/api/connect/requests", json=GOOD_BODY).status_code == 201
    assert len(connect_requests._requests) == 1


def test_c1_a_claimed_request_does_not_count_as_pending(anon, admin):
    made = [make_request(anon) for _ in range(20)]
    admin.post(approve_url(made[0]["user_code"]))
    assert claim(anon, made[0]["request_id"]).json()["status"] == "approved"
    assert anon.post("/api/connect/requests", json=GOOD_BODY).status_code == 201


def test_c1_codes_are_unique_among_pending_requests(anon, monkeypatch):
    codes = iter(["AAAAAAAA", "AAAAAAAA", "BBBBBBBB"])
    monkeypatch.setattr(connect_requests, "_random_code", lambda: next(codes))
    first = make_request(anon)
    second = make_request(anon)
    assert first["user_code"] == "AAAA-AAAA"
    assert second["user_code"] == "BBBB-BBBB"


def test_c1_twenty_requests_give_twenty_different_codes(anon):
    codes = {make_request(anon)["user_code"] for _ in range(20)}
    assert len(codes) == 20
    assert all(CODE_PATTERN.match(code) for code in codes)


def test_c1_lifetime_comes_from_the_setting(anon, monkeypatch):
    monkeypatch.setattr(settings, "connect_request_lifetime_seconds", 30)
    assert make_request(anon)["expires_in_seconds"] == 30


def test_lifetime_setting_has_the_agreed_name_and_default(monkeypatch):
    monkeypatch.delenv("EWS_CONNECT_REQUEST_LIFETIME_SECONDS", raising=False)
    assert Settings(_env_file=None).connect_request_lifetime_seconds == 600
    monkeypatch.setenv("EWS_CONNECT_REQUEST_LIFETIME_SECONDS", "45")
    assert Settings(_env_file=None).connect_request_lifetime_seconds == 45


# ---- C2 ----

def test_c2_needs_the_session(anon):
    made = make_request(anon)
    response = anon.get(info_url(made["user_code"]))
    assert response.status_code == 401
    assert response.headers["cache-control"] == "no-store"


def test_c2_answer(anon, admin, clock):
    made = make_request(anon)
    clock.advance(188)
    response = admin.get(info_url(made["user_code"]))
    assert response.status_code == 200
    assert response.json() == {
        "user_code": made["user_code"], "device_id": DEVICE_ID, "device_name": "Lab 1 PC 03",
        "os": "windows", "device_type": "laptop", "approved": False, "expires_in_seconds": 412,
    }
    assert response.headers["cache-control"] == "no-store"
    assert "request_id" not in response.json()


@pytest.mark.parametrize("write", [str.lower, lambda code: code.replace("-", ""), lambda code: code.lower().replace("-", ""), str.upper])
def test_c2_code_is_compared_without_case_and_hyphen(anon, admin, write):
    made = make_request(anon)
    response = admin.get(info_url(write(made["user_code"])))
    assert response.status_code == 200
    assert response.json()["user_code"] == made["user_code"]


@pytest.mark.parametrize("code", ["ZZZZ-ZZZZ", "ABC", "ABCD-EFG0", "ABCD-EFG1", "IIII-OOOO", "ABCD-EFGHJ", "ABCD EFGH", "ABCD%20EFG"])
def test_c2_unknown_or_malformed_code_is_404(anon, admin, code):
    make_request(anon)
    response = admin.get(info_url(code))
    assert response.status_code == 404
    assert response.json() == NOT_FOUND
    assert response.headers["cache-control"] == "no-store"


def test_c2_expired_request_is_404(anon, admin, clock):
    made = make_request(anon)
    clock.advance(600)
    assert admin.get(info_url(made["user_code"])).status_code == 404


def test_c2_claimed_request_is_404(anon, admin):
    made = make_request(anon)
    admin.post(approve_url(made["user_code"]))
    claim(anon, made["request_id"])
    assert admin.get(info_url(made["user_code"])).json() == NOT_FOUND


def test_c2_on_an_empty_store_is_404(admin):
    assert admin.get(info_url("ABCD-EFGH")).json() == NOT_FOUND


# ---- C3 ----

def test_c3_needs_the_session(anon):
    made = make_request(anon)
    response = anon.post(approve_url(made["user_code"]))
    assert response.status_code == 401
    assert response.headers["cache-control"] == "no-store"
    assert claim(anon, made["request_id"]).json() == {"status": "pending"}


def test_c3_approves_and_repeating_is_safe(anon, admin):
    made = make_request(anon)
    first = admin.post(approve_url(made["user_code"]))
    second = admin.post(approve_url(made["user_code"]))
    assert first.status_code == 204 and first.content == b""
    assert second.status_code == 204
    assert first.headers["cache-control"] == "no-store"
    assert admin.get(info_url(made["user_code"])).json()["approved"] is True


@pytest.mark.parametrize("write", [str.lower, lambda code: code.replace("-", "")])
def test_c3_code_is_compared_without_case_and_hyphen(anon, admin, write):
    made = make_request(anon)
    assert admin.post(approve_url(write(made["user_code"]))).status_code == 204
    assert claim(anon, made["request_id"]).json()["status"] == "approved"


@pytest.mark.parametrize("code", ["ZZZZ-ZZZZ", "ABC", "ABCD-EFG0"])
def test_c3_unknown_or_malformed_code_is_404(admin, code):
    response = admin.post(approve_url(code))
    assert response.status_code == 404
    assert response.json() == NOT_FOUND
    assert response.headers["cache-control"] == "no-store"


def test_c3_expired_request_is_404(anon, admin, clock):
    made = make_request(anon)
    clock.advance(601)
    assert admin.post(approve_url(made["user_code"])).status_code == 404


def test_c3_approves_only_the_named_request(anon, admin):
    first = make_request(anon)
    second = make_request(anon)
    admin.post(approve_url(first["user_code"]))
    assert claim(anon, second["request_id"]).json() == {"status": "pending"}


# ---- C4 ----

def test_c4_pending_until_the_admin_approves(anon):
    made = make_request(anon)
    for _ in range(3):
        response = claim(anon, made["request_id"])
        assert response.status_code == 200
        assert response.json() == {"status": "pending"}
        assert response.headers["cache-control"] == "no-store"


def test_c4_approved_gives_the_key_once(anon, admin):
    made = make_request(anon)
    admin.post(approve_url(made["user_code"]))
    first = claim(anon, made["request_id"])
    assert first.status_code == 200
    assert first.json() == {"status": "approved", "client_key": CLIENT_KEY}
    assert first.headers["cache-control"] == "no-store"
    second = claim(anon, made["request_id"])
    assert second.status_code == 404
    assert second.json() == NOT_FOUND
    assert second.headers["cache-control"] == "no-store"
    assert connect_requests._requests == {}


def test_c4_unknown_request_is_404(anon):
    response = claim(anon, UNKNOWN_ID)
    assert response.status_code == 404
    assert response.json() == NOT_FOUND


def test_c4_expired_request_is_404(anon, clock):
    made = make_request(anon)
    clock.advance(600)
    assert claim(anon, made["request_id"]).json() == NOT_FOUND


def test_c4_request_that_expired_after_the_approval_is_404_without_the_key(anon, admin, clock):
    made = make_request(anon)
    admin.post(approve_url(made["user_code"]))
    clock.advance(600)
    response = claim(anon, made["request_id"])
    assert response.status_code == 404
    assert CLIENT_KEY not in response.text


@pytest.mark.parametrize("request_id", ["short", "x" * 19, "x" * 101, "", 12345])
def test_c4_request_id_length_is_checked(anon, request_id):
    assert claim(anon, request_id).status_code == 422


def test_c4_unknown_field_is_422(anon):
    response = anon.post("/api/connect/claim", json={"request_id": UNKNOWN_ID, "user_code": "ABCD-EFGH"})
    assert response.status_code == 422


def test_c4_the_secret_is_not_taken_from_the_address(anon):
    made = make_request(anon)
    assert anon.post(f"/api/connect/claim?request_id={made['request_id']}").status_code == 422


def test_c4_after_the_key_was_removed_is_503_and_keeps_the_request(anon, admin, monkeypatch):
    made = make_request(anon)
    admin.post(approve_url(made["user_code"]))
    monkeypatch.setattr(settings, "client_key", "")
    response = claim(anon, made["request_id"])
    assert response.status_code == 503
    assert response.json() == KEY_MISSING
    assert response.headers["cache-control"] == "no-store"
    monkeypatch.setattr(settings, "client_key", CLIENT_KEY)
    assert claim(anon, made["request_id"]).json()["status"] == "approved"


def test_c4_gives_the_key_now_in_the_settings(anon, admin, monkeypatch):
    made = make_request(anon)
    admin.post(approve_url(made["user_code"]))
    monkeypatch.setattr(settings, "client_key", "rotated-key-9876543210")
    assert claim(anon, made["request_id"]).json()["client_key"] == "rotated-key-9876543210"


def test_c4_on_an_empty_store_is_404(anon):
    assert claim(anon, UNKNOWN_ID).status_code == 404


# ---- All four ----

def test_nothing_of_a_request_is_logged(anon, admin, caplog):
    caplog.set_level(logging.DEBUG)
    made = make_request(anon)
    admin.get(info_url(made["user_code"]))
    admin.post(approve_url(made["user_code"]))
    claim(anon, made["request_id"])
    claim(anon, made["request_id"])
    admin.get(info_url("ZZZZ-ZZZZ"))
    secrets_in_play = [made["request_id"], made["user_code"], made["user_code"].replace("-", ""), CLIENT_KEY]
    # The test client logs the addresses it calls (httpx2). That is the client side, not the server.
    server_records = [record for record in caplog.records if not record.name.startswith(("httpx", "asyncio"))]
    logged = "\n".join(record.getMessage() for record in server_records)
    assert all(secret not in logged for secret in secrets_in_play)
    assert DEVICE_ID not in logged


def test_a_restart_drops_the_requests(anon, admin, monkeypatch):
    made = make_request(anon)
    monkeypatch.setattr(connect_requests, "_requests", {})
    assert admin.get(info_url(made["user_code"])).status_code == 404
    assert claim(anon, made["request_id"]).status_code == 404
