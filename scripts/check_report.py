#!/usr/bin/env python3
"""Check report files against shared/report_schema.json.

What it is for
    A client sends reports to the server. The shape of a report is fixed in
    shared/report_schema.json. This script tells you, before you merge, if an
    example file (or any file you give it) follows that shape. CI runs it on
    every pull request, so a broken example or schema is found early.

Who uses it
    Every developer on a laptop, and the CI workflow (.github/workflows/ci.yml).

How to run it (from any folder)
    pip install -r scripts/requirements-check.txt
    python scripts/check_report.py                 # the files in shared/examples
    python scripts/check_report.py a.json          # one file
    python scripts/check_report.py a.json b.json   # several files

    A file holds ONE report (a JSON object) or a LIST of reports.

Output, one line per report
    OK <file>
    FAIL <file> [index N] <path in the report>: <problem>
    The last line is a summary: "N reports checked, M failed".
    A report with several problems prints one FAIL line for each problem.

Exit codes
    0  every report is valid
    1  at least one report is invalid
    2  usage or file problem (file missing, not valid JSON, schema missing,
       or no reports found to check); the reason is printed on stderr
"""

import json
import sys
from datetime import datetime
from pathlib import Path

from jsonschema import Draft202012Validator

# The schema is found from the place of this script, not from the current
# folder. This way the script works when started from anywhere.
SCHEMA_PATH = Path(__file__).resolve().parent.parent / "shared" / "report_schema.json"
EXAMPLES_DIR = SCHEMA_PATH.parent / "examples"


class UsageError(Exception):
    """A problem with the files or the arguments (exit code 2)."""


def load_json(path):
    """Read a JSON file and return its data. Raise UsageError if it fails.

    Why: a missing file or broken JSON is a mistake of the user, not a bad
    report. It must give exit code 2 and a clear reason, not a stack trace.
    """
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise UsageError(f"cannot read {path}: {error}")


def make_validator():
    """Build the validator from the schema file.

    The format checker makes the "uuid" format real. Without it the schema
    would accept any string as a device id.
    """
    schema = load_json(SCHEMA_PATH)
    return Draft202012Validator(schema, format_checker=Draft202012Validator.FORMAT_CHECKER)


def reports_in(data):
    """Return the reports of a file as a list: a list stays, one object is wrapped."""
    return data if isinstance(data, list) else [data]


def has_time_zone(text):
    """Say if a timestamp text is a real date with a time zone.

    A trailing Z means UTC, but fromisoformat reads it only as "+00:00" on
    older Python versions, so we replace it first.
    """
    try:
        moment = datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError:
        return False
    return moment.tzinfo is not None


def zone_problems(report):
    """Return the extra timestamp problem, if there is one.

    Why this check exists: the schema says "date-time", but jsonschema checks
    that format only when the package rfc3339-validator is installed. We do
    not want a missing package to let a time without a zone pass, because the
    server stores UTC. So the script checks the zone itself.
    """
    timestamp = report.get("timestamp") if isinstance(report, dict) else None
    if not isinstance(timestamp, str) or has_time_zone(timestamp):
        return []
    return [("timestamp", "needs a time zone")]


def error_path(error):
    """Return where in the report an error is, for example "timestamp"."""
    return "/".join(str(part) for part in error.absolute_path) or "(report)"


def check_one(validator, report):
    """Return the problems of one report as sorted (path, text) pairs.

    Uses iter_errors so that all problems are shown, not only the first one.
    """
    errors = sorted(validator.iter_errors(report), key=error_path)
    found = [(error_path(error), error.message) for error in errors]
    return sorted(found + zone_problems(report))


def report_lines(path, label, problems):
    """Return the output lines of one report: one OK line, or one FAIL line per problem."""
    if not problems:
        return [f"OK {path}{label}"]
    return [f"FAIL {path}{label} {where}: {text}" for where, text in problems]


def check_file(validator, path):
    """Check every report in a file.

    Returns (lines, checked, failed): the output lines, the number of
    reports and the number of invalid reports.
    """
    data = load_json(path)
    reports = reports_in(data)
    # The index is shown only for a list, because a single object has none.
    is_list = isinstance(data, list)
    results = [check_one(validator, report) for report in reports]
    labels = [f" [index {i}]" if is_list else "" for i in range(len(reports))]
    lines = [line for label, problems in zip(labels, results) for line in report_lines(path, label, problems)]
    return lines, len(reports), sum(1 for problems in results if problems)


def default_files():
    """Return the example reports in shared/examples, in a stable order."""
    return sorted(str(path) for path in EXAMPLES_DIR.glob("*.json"))


def require_reports(results):
    """Raise UsageError if no report was found to check.

    Why: "0 reports checked, 0 failed" with exit code 0 looks like a pass, but
    nothing was checked. This happens when shared/examples is empty or moved,
    or when a file holds an empty list. CI must not go green in that case.
    """
    if sum(count for _, count, _ in results) == 0:
        raise UsageError("no reports found, nothing was checked")


def main(argv):
    """Run the checker for the files in argv (or the examples). Return the exit code."""
    paths = argv or default_files()
    try:
        validator = make_validator()
        results = [check_file(validator, path) for path in paths]
        require_reports(results)
    except UsageError as error:
        print(error, file=sys.stderr)
        return 2
    for lines, _, _ in results:
        print("\n".join(lines))
    checked = sum(count for _, count, _ in results)
    failed = sum(bad for _, _, bad in results)
    print(f"{checked} reports checked, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
