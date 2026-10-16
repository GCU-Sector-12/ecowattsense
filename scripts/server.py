#!/usr/bin/env python3
"""Run or test the EcoWattSense server with one command.

Works on Windows, macOS and Linux. Creates server/.venv on first use and installs
server/requirements.txt into it, so nobody has to remember the pip and uvicorn commands.

    python scripts/server.py run          start the server on http://localhost:8000 (auto reload)
    python scripts/server.py run --port 8080
    python scripts/server.py test         run the pytest suite in server/tests
    python scripts/server.py test -k health -v   (extra arguments go to pytest)
    python scripts/server.py install      only create the venv and install requirements

Needs Python 3.11 or newer on PATH (python3 on macOS and Linux, python on Windows).
"""

import hashlib
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / "server"
VENV = SERVER / ".venv"
REQUIREMENTS = SERVER / "requirements.txt"
STAMP = VENV / ".requirements.sha"


def venv_python() -> Path:
    if os.name == "nt":
        return VENV / "Scripts" / "python.exe"
    return VENV / "bin" / "python"


def run(cmd, **kwargs) -> int:
    print("$", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.call([str(c) for c in cmd], **kwargs)


def create_venv(py: Path) -> None:
    if py.exists():
        return
    print(f"Creating virtual environment in {VENV}")
    code = run([sys.executable, "-m", "venv", VENV])
    if code != 0:
        sys.exit("Could not create the virtual environment.")


def requirements_installed(digest: str) -> bool:
    if not STAMP.exists():
        return False
    return STAMP.read_text().strip() == digest


def install_requirements(py: Path) -> None:
    digest = hashlib.sha256(REQUIREMENTS.read_bytes()).hexdigest()
    if requirements_installed(digest):
        return
    print("Installing requirements (first run or requirements.txt changed)")
    code = run([py, "-m", "pip", "install", "-q", "-r", REQUIREMENTS])
    if code != 0:
        sys.exit("pip install failed. Check your network and the Python version.")
    STAMP.write_text(digest)


def ensure_venv() -> Path:
    """Create the venv and install requirements when missing or changed."""
    py = venv_python()
    create_venv(py)
    install_requirements(py)
    return py


def port_from(args) -> str:
    """The value after --port, or 8000. Other arguments go to uvicorn as they are."""
    if "--port" not in args:
        return "8000"
    index = args.index("--port") + 1
    if index >= len(args):
        return "8000"
    return args[index]


def cmd_run(args) -> int:
    py = ensure_venv()
    port = port_from(args)
    print(f"\nServer: http://localhost:{port}   health check: http://localhost:{port}/api/health")
    print("Stop with Ctrl + C\n")
    uvicorn = [py, "-m", "uvicorn", "ecowattsense_server.main:app", "--reload", "--port", port]
    rest = [a for a in args if a != "--port" and a != port]
    return run([*uvicorn, *rest], cwd=SERVER)


def cmd_test(args) -> int:
    py = ensure_venv()
    return run([py, "-m", "pytest", *args], cwd=SERVER)


def cmd_install(_args) -> int:
    ensure_venv()
    print("Done.")
    return 0


COMMANDS = {"run": cmd_run, "test": cmd_test, "install": cmd_install}


def main() -> int:
    if sys.version_info < (3, 11):
        sys.exit(f"Python 3.11 or newer is needed, you have {sys.version.split()[0]}.")
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        print(__doc__)
        return 2
    return COMMANDS[sys.argv[1]](sys.argv[2:])


if __name__ == "__main__":
    sys.exit(main())
