import os

import httpx
import pytest


BASE_URL = os.getenv("EWS_SMOKE_BASE")
CLIENT_KEY = os.getenv("EWS_CLIENT_KEY")
ADMIN_USERNAME = os.getenv("EWS_ADMIN_USERNAME")
ADMIN_PASSWORD = os.getenv("EWS_ADMIN_PASSWORD")

pytestmark = pytest.mark.skipif(
    not BASE_URL or not CLIENT_KEY,
    reason="EWS_SMOKE_BASE and EWS_CLIENT_KEY are required for staging smoke tests",
)


def test_health():
    response = httpx.get(f"{BASE_URL}/api/health", timeout=10)
    assert response.status_code == 200


def test_settings_with_key():
    response = httpx.get(
        f"{BASE_URL}/api/settings",
        headers={"X-EWS-Key": CLIENT_KEY},
        timeout=10,
    )

    assert response.status_code == 200

    data = response.json()

    required_fields = {
        "settings_version",
        "report_interval_seconds",
        "ping_interval_seconds",
        "idle_threshold_seconds",
        "unused_threshold_seconds",
        "no_data_after_cycles",
        "tariff_valid_from",
        "price_gbp_per_kwh",
        "co2_kg_per_kwh",
    }

    assert required_fields.issubset(data.keys())


def test_settings_without_key():
    response = httpx.get(f"{BASE_URL}/api/settings", timeout=10)
    assert response.status_code == 401



@pytest.mark.skipif(
    not ADMIN_USERNAME or not ADMIN_PASSWORD,
    reason="EWS_ADMIN_USERNAME and EWS_ADMIN_PASSWORD are required",
)
def test_admin_login_and_devices():
    with httpx.Client(base_url=BASE_URL, timeout=10) as client:
        login = client.post(
            "/api/login",
            json={
                "username": ADMIN_USERNAME,
                "password": ADMIN_PASSWORD,
            },
        )

        assert login.status_code == 200
        assert "ews_session" in client.cookies

        devices = client.get("/api/devices")
        assert devices.status_code == 200

        data = devices.json()
        assert "devices" in data


def test_devices_without_cookie():
    response = httpx.get(f"{BASE_URL}/api/devices", timeout=10)
    assert response.status_code == 401


# One fixed test computer. Its report has a fixed timestamp, so running the smoke test again
# gives "duplicate" and adds no new row to staging.
SMOKE_DEVICE_ID = "5e0b6f2a-3c1d-4b7e-9a48-0d2f6c1b7e93"
SMOKE_REPORT = {
    "device_id": SMOKE_DEVICE_ID,
    "timestamp": "2026-11-01T12:00:00Z",
    "interval_seconds": 300,
    "state": 2,
    "idle_seconds": 0,
    "cpu_percent": 1.0,
    "screen_locked": False,
    "on_battery": False,
    "os": "linux",
    "device_type": "desktop",
}

needs_admin = pytest.mark.skipif(
    not ADMIN_USERNAME or not ADMIN_PASSWORD,
    reason="EWS_ADMIN_USERNAME and EWS_ADMIN_PASSWORD are required",
)


@pytest.fixture
def admin_client():
    """An HTTP client that is logged in as the admin."""
    with httpx.Client(base_url=BASE_URL, timeout=10) as client:
        login = client.post("/api/login", json={"username": ADMIN_USERNAME, "password": ADMIN_PASSWORD})
        assert login.status_code == 200
        yield client


@needs_admin
def test_admin_summary(admin_client):
    response = admin_client.get("/api/summary")
    assert response.status_code == 200
    assert "counts" in response.json()


@needs_admin
def test_a_posted_report_shows_the_device_in_devices(admin_client):
    posted = httpx.post(f"{BASE_URL}/api/reports", json=SMOKE_REPORT, headers={"X-EWS-Key": CLIENT_KEY}, timeout=10)
    assert posted.status_code in (200, 201)
    devices = admin_client.get("/api/devices").json()["devices"]
    assert SMOKE_DEVICE_ID in [device["device_id"] for device in devices]
