"""Actual loopback HTTP smoke tests; no external server or third-party packages."""
import contextlib
from datetime import datetime, timedelta, timezone
import http.client
import io
import json
import threading
import unittest

from mock_server import DUMMY_KEY, MAX_BODY, MockServer, SETTINGS


class MockSmokeTests(unittest.TestCase):
    def setUp(self):
        self.output = io.StringIO()
        self.capture = contextlib.redirect_stdout(self.output)
        self.capture.__enter__()
        self.server = MockServer(0)
        self.thread = threading.Thread(target=self.server.serve_forever, kwargs={"poll_interval": 0.01})
        self.thread.start()
        self.report = {
            "device_id": "8f20c31c-77ed-446d-a681-7717e3f62c25",
            "timestamp": "2026-01-01T12:00:00Z", "interval_seconds": 15,
            "state": 2, "idle_seconds": 0, "cpu_percent": 37.5,
            "screen_locked": False, "on_battery": True, "os": "windows", "device_type": "laptop"}

    def tearDown(self):
        self.server.shutdown()
        self.thread.join(timeout=5)
        self.server.server_close()
        self.capture.__exit__(None, None, None)

    def request(self, method, path, body=None, key=DUMMY_KEY):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=5)
        headers = {"Content-Type": "application/json"}
        if key is not None:
            headers["X-EWS-Key"] = key
        try:
            connection.request(method, path, body, headers)
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def test_settings_and_authentication(self):
        self.assertEqual(self.server.server_address[0], "127.0.0.1")
        self.assertEqual(self.request("GET", "/api/settings"), (200, SETTINGS))
        for key in (None, "wrong-key-not-to-be-logged"):
            for method, path, body in (("GET", "/api/settings", None), ("POST", "/api/reports", json.dumps(self.report))):
                self.assertEqual(self.request(method, path, body, key)[0], 401)
        self.assertNotIn("wrong-key-not-to-be-logged", self.output.getvalue())
        self.assertEqual(len(self.server.reports), 0)

    def test_acceptance_duplicates_and_memory(self):
        body = json.dumps(self.report)
        self.assertEqual(self.request("POST", "/api/reports", body),
                         (201, {"results": [{"index": 0, "status": "created", "error": None}]}))
        self.assertEqual(self.request("POST", "/api/reports", body),
                         (200, {"results": [{"index": 0, "status": "duplicate", "error": None}]}))
        equivalent = dict(self.report, timestamp="2026-01-01T12:00:00.0000000Z")
        self.assertEqual(self.request("POST", "/api/reports", json.dumps(equivalent))[0], 200)
        self.assertEqual(len(self.server.reports), 1)
        self.assertEqual(json.loads(self.output.getvalue()), self.report)
        self.assertNotIn(DUMMY_KEY, self.output.getvalue())
        # A separate server starts empty: no disk storage or process-global reports.
        with MockServer(0) as fresh:
            self.assertEqual(fresh.reports, {})

    def test_invalid_values(self):
        cases = {
            "device_id": [None, True, "bad", "00000000-0000-0000-0000-000000000000", self.report["device_id"].upper()],
            "timestamp": [None, "2026-01-01T12:00:00", "2026-02-30T12:00:00Z", "2026-01-01T12:00:00+01:00",
                          (datetime.now(timezone.utc) + timedelta(minutes=10)).isoformat().replace("+00:00", "Z")],
            "interval_seconds": [True, 0, -1, 1.5, "15"],
            "state": [True, -1, 3, "2"], "idle_seconds": [False, -1, 0.5, "0"],
            "cpu_percent": [True, -1, 101, "50", float("nan"), float("inf")],
            "screen_locked": [0, "false", None], "on_battery": [1, "true", None],
            "os": [None, "Windows", "other", []],
            "device_type": [None, "", "Laptop", "laptop\n", "1pc", "a" * 41]}
        for field, values in cases.items():
            for value in values:
                with self.subTest(field=field, value=value):
                    report = dict(self.report, **{field: value})
                    self.assertEqual(self.request("POST", "/api/reports", json.dumps(report))[0], 422)
        self.assertEqual(len(self.server.reports), 0)

    def test_shape_json_and_size(self):
        for field in self.report:
            body = self.report.copy()
            del body[field]
            self.assertEqual(self.request("POST", "/api/reports", json.dumps(body))[0], 422)
        for extra in ("unexpected", "energy_uj", "machine_hash"):
            self.assertEqual(self.request("POST", "/api/reports", json.dumps(dict(self.report, **{extra: 0})))[0], 422)
        for body in ("{", "null", "[]", '[{}]', '{"state":0,"state":2}', "x" * (MAX_BODY + 1), b'\xff'):
            self.assertEqual(self.request("POST", "/api/reports", body)[0], 422)
        self.assertEqual(self.request("POST", "/api/reports", json.dumps(dict(self.report, device_type="desktop")))[0], 422)

    def test_valid_boundaries(self):
        for index, (state, cpu) in enumerate(((0, 0), (1, 100), (2, 50))):
            body = dict(self.report, state=state, cpu_percent=cpu, interval_seconds=1,
                        timestamp=f"2026-01-01T12:00:0{index}Z", on_battery=False, device_type="desktop")
            self.assertEqual(self.request("POST", "/api/reports", json.dumps(body))[0], 201)


if __name__ == "__main__":
    unittest.main(verbosity=2)
