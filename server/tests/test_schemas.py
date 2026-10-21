# server/tests/test_schemas.py
import json
from datetime import datetime, timedelta, timezone
from pathlib import Path

import pytest
from pydantic import TypeAdapter, ValidationError

from ecowattsense_server.schemas import ReportBody, ReportIn, ReportResult, ReportResults

EXAMPLES = Path(__file__).resolve().parents[2] / "shared" / "examples"
body = TypeAdapter(ReportBody)


def example(name):
    return json.loads((EXAMPLES / f"{name}.json").read_text(encoding="utf-8"))


def valid(**changes):
    """A valid report (the active example) with some fields changed. None removes a field."""
    report = example("active")
    for key, value in changes.items():
        if value is None:
            report.pop(key, None)
        else:
            report[key] = value
    return report


@pytest.mark.parametrize("name", ["active", "idle", "unused"])
def test_examples_pass(name):
    assert ReportIn.model_validate(example(name)).device_type in ("desktop", "laptop")


# One wrong thing at a time. The id is the name of the case in the test report.
BAD_REPORTS = {
    "missing on_battery": valid(on_battery=None),
    "unknown field": valid(colour="green"),
    "state 3": valid(state=3),
    "time without a zone": valid(timestamp="2026-10-19T09:05:00"),
    "negative energy_uj": valid(energy_uj=-1),
    "bad machine_hash": valid(machine_hash="abc"),
    "upper case machine_hash": valid(machine_hash="9F" + "0" * 62),
    "device_type Laptop": valid(device_type="Laptop"),
    "device_type too long": valid(device_type="a" * 41),
    "cpu_percent 101": valid(cpu_percent=101),
    "cpu_percent negative": valid(cpu_percent=-0.1),
    "interval_seconds 0": valid(interval_seconds=0),
    "negative idle_seconds": valid(idle_seconds=-1),
    "unknown os": valid(os="beos"),
    "device_id not a uuid": valid(device_id="not-a-uuid"),
    "energy_uj null": valid(energy_uj=None) | {"energy_uj": None},
    "on battery with energy_uj 5000000": valid(on_battery=True, energy_uj=5_000_000),
    "timestamp 10 minutes ahead": valid(
        timestamp=(datetime.now(timezone.utc) + timedelta(minutes=10)).isoformat()
    ),
}


@pytest.mark.parametrize("report", BAD_REPORTS.values(), ids=BAD_REPORTS.keys())
def test_wrong_reports_fail(report):
    with pytest.raises(ValidationError):
        ReportIn.model_validate(report)


def test_on_battery_with_zero_energy_passes():
    assert ReportIn.model_validate(valid(on_battery=True, energy_uj=0)).energy_uj == 0


def test_battery_energy_can_be_negative():
    assert ReportIn.model_validate(valid(battery_energy_uj=-2_700_000_000)).battery_energy_uj < 0


def test_timestamp_a_little_ahead_passes():
    soon = (datetime.now(timezone.utc) + timedelta(minutes=2)).isoformat()
    ReportIn.model_validate(valid(timestamp=soon))


def test_body_is_one_report():
    assert isinstance(body.validate_python(example("active")), ReportIn)


def test_list_of_two_valid_reports_passes():
    reports = body.validate_python([example("idle"), example("unused")])
    assert len(reports) == 2


def test_list_of_100_passes():
    assert len(body.validate_python([example("active")] * 100)) == 100


def test_empty_list_fails():
    with pytest.raises(ValidationError):
        body.validate_python([])


def test_list_of_101_fails():
    with pytest.raises(ValidationError):
        body.validate_python([example("active")] * 101)


def test_list_with_a_wrong_report_fails():
    with pytest.raises(ValidationError):
        body.validate_python([example("active"), valid(state=3)])


def test_results_reply_shape():
    reply = ReportResults(
        results=[
            ReportResult(index=0, status="created"),
            ReportResult(index=1, status="rejected", error="state must be 2, 1 or 0"),
        ]
    )
    assert reply.model_dump() == {
        "results": [
            {"index": 0, "status": "created", "error": None},
            {"index": 1, "status": "rejected", "error": "state must be 2, 1 or 0"},
        ]
    }


def test_result_status_must_be_known():
    with pytest.raises(ValidationError):
        ReportResult(index=0, status="stored")


def test_model_matches_the_schema_fields():
    """Same field names and the same required fields as shared/report_schema.json."""
    schema = json.loads((EXAMPLES.parent / "report_schema.json").read_text(encoding="utf-8"))
    fields = ReportIn.model_fields
    assert set(fields) == set(schema["properties"])
    required = {name for name, field in fields.items() if field.is_required()}
    assert required == set(schema["required"])
