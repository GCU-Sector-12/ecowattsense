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
        "report_interval_seconds",
        "idle_threshold_seconds",
        "unused_threshold_seconds",
        "settings_version",
        "device_types",
        "states",
        "report_fields",
        "api_version",
        "server_time",
    }

    assert required_fields.issubset(data.keys())


def test_settings_without_key():
    response = httpx.get(f"{BASE_URL}/api/settings", timeout=10)
    assert response.status_code == 401


def test_reports_with_key():
    examples = [
        "report_active.json",
        "report_idle.json",
        "report_unused.json",
    ]

    for filename in examples:
        path = os.path.join(
            os.path.dirname(__file__),
            "..",
            "..",
            "shared",
            "examples",
            filename,
        )

        with open(path, encoding="utf-8") as file:
            report = __import__("json").load(file)

        response = httpx.post(
            f"{BASE_URL}/api/reports",
            headers={"X-EWS-Key": CLIENT_KEY},
            json=report,
            timeout=10,
        )

        assert response.status_code in (200, 201)

        data = response.json()
        assert "results" in data

        for result in data["results"]:
            assert result["status"] in ("created", "duplicate")


def test_reports_without_key():
    response = httpx.post(
        f"{BASE_URL}/api/reports",
        json={},
        timeout=10,
    )

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