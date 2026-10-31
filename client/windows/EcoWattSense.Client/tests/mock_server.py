"""Loopback-only TEST MOCK, not the EcoWattSense backend. Standard library only."""

import argparse
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import re
from uuid import UUID

DUMMY_KEY = "local-test-key"
MAX_BODY = 16 * 1024
FIELDS = {"device_id", "timestamp", "interval_seconds", "state", "idle_seconds",
          "cpu_percent", "screen_locked", "on_battery", "os", "device_type"}
SETTINGS = {"report_interval_seconds": 15, "idle_threshold_seconds": 10,
            "unused_threshold_seconds": 20, "settings_version": 1}


def validate_report(report):
    """Validate the current client's ten-field subset; return a canonical duplicate key."""
    if type(report) is not dict or set(report) != FIELDS:
        raise ValueError("Expected one report object with exactly the ten required fields.")
    device_id = report["device_id"]
    if not isinstance(device_id, str):
        raise ValueError("device_id must be a UUID string.")
    try:
        parsed_id = UUID(device_id)
    except ValueError:
        raise ValueError("device_id must be a UUID string.") from None
    if parsed_id.int == 0 or str(parsed_id) != device_id:
        raise ValueError("device_id must be a nonempty lowercase hyphenated UUID.")
    timestamp = report["timestamp"]
    if not isinstance(timestamp, str) or not re.fullmatch(
            r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z", timestamp):
        raise ValueError("timestamp must be an ISO UTC timestamp ending in Z.")
    try:
        instant = datetime.fromisoformat(timestamp[:-1] + "+00:00")
    except ValueError:
        raise ValueError("timestamp contains an invalid date or time.") from None
    if instant > datetime.now(timezone.utc) + timedelta(minutes=5):
        raise ValueError("timestamp is more than five minutes in the future.")
    for field, minimum in (("interval_seconds", 1), ("idle_seconds", 0)):
        if type(report[field]) is not int or report[field] < minimum:
            raise ValueError(f"{field} must be an integer >= {minimum}.")
    if type(report["state"]) is not int or report["state"] not in (0, 1, 2):
        raise ValueError("state must be integer 0, 1 or 2.")
    cpu = report["cpu_percent"]
    if type(cpu) not in (int, float) or not 0 <= cpu <= 100:
        raise ValueError("cpu_percent must be a finite number between 0 and 100.")
    for field in ("screen_locked", "on_battery"):
        if type(report[field]) is not bool:
            raise ValueError(f"{field} must be boolean.")
    if report["os"] not in ("windows", "linux", "macos"):
        raise ValueError("os must be windows, linux or macos.")
    device_type = report["device_type"]
    if not isinstance(device_type, str) or len(device_type) > 40 or not re.fullmatch(r"[a-z][a-z0-9_]*", device_type):
        raise ValueError("device_type must be a lowercase identifier of at most 40 characters.")
    if device_type == "desktop" and report["on_battery"]:
        raise ValueError("A desktop report must have on_battery false.")
    # Preserve sub-microsecond precision from .NET; normalize only trailing fractional zeroes.
    seconds, _, fraction = timestamp[:-1].partition(".")
    return device_id, seconds, fraction.rstrip("0")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON property.")
        result[key] = value
    return result


def reject_constant(_):
    raise ValueError("Non-finite JSON numbers are invalid.")


class MockServer(HTTPServer):
    def __init__(self, port=8765):
        super().__init__(("127.0.0.1", port), MockHandler)
        self.reports = {}


class MockHandler(BaseHTTPRequestHandler):
    server_version = "EcoWattSense-Test-Mock/1"

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def log_message(self, *_):
        pass  # Never log request headers, URLs, or supplied authentication keys.

    def respond(self, status, body):
        payload = json.dumps(body, allow_nan=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        try:
            self.wfile.write(payload)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def authenticated(self):
        if self.headers.get_all("X-EWS-Key", []) != [DUMMY_KEY]:
            self.respond(401, {"detail": "Missing or invalid test key."})
            return False
        return True

    def do_GET(self):
        if not self.authenticated():
            return
        if self.path == "/api/settings":
            self.respond(200, SETTINGS)
        else:
            self.respond(404, {"detail": "Test mock endpoint not found."})

    def do_POST(self):
        if not self.authenticated():
            return
        if self.path != "/api/reports":
            self.respond(404, {"detail": "Test mock endpoint not found."})
            return
        try:
            lengths = self.headers.get_all("Content-Length", [])
            if self.headers.get("Transfer-Encoding") or len(lengths) != 1 or not re.fullmatch(r"[0-9]+", lengths[0]):
                raise ValueError("One Content-Length is required; chunked bodies are unsupported.")
            length = int(lengths[0])
            if not 0 < length <= MAX_BODY:
                raise ValueError("Request body must be between 1 and 16384 bytes.")
            data = self.rfile.read(length)
            if len(data) != length:
                raise ValueError("Incomplete request body.")
            report = json.loads(data.decode("utf-8"), object_pairs_hook=unique_object, parse_constant=reject_constant)
            key = validate_report(report)
        except (ValueError, UnicodeError, RecursionError, TimeoutError):
            self.respond(422, {"detail": "Invalid JSON or report: check the ten-field contract, types, values and body size."})
            return
        duplicate = key in self.server.reports
        if not duplicate:
            self.server.reports[key] = report
            print(json.dumps(report, allow_nan=False), flush=True)
        self.respond(200 if duplicate else 201, {"results": [
            {"index": 0, "status": "duplicate" if duplicate else "created", "error": None}]})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be between 1 and 65535")
    with MockServer(args.port) as server:
        print(f"TEST MOCK ONLY — not the real backend. Listening on http://127.0.0.1:{args.port}", flush=True)
        print("Accepted reports are in memory only; all data disappears when this mock stops. Ctrl+C to stop.", flush=True)
        try:
            server.serve_forever(poll_interval=0.2)
        except KeyboardInterrupt:
            print("\nTest mock stopped; in-memory reports discarded.", flush=True)


if __name__ == "__main__":
    main()
