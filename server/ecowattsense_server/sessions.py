"""Expiring, revocable sessions for the single-process W6 server.

Only opaque random tokens go in cookies. Restarting the server ends all sessions.
A shared session store is needed before running more than one worker.
"""
import secrets
from threading import Lock
from time import monotonic

from .config import settings

_sessions: dict[str, tuple[str, float]] = {}
_lock = Lock()


def create_session(username: str) -> str:
    token = secrets.token_urlsafe(32)
    now = monotonic()
    with _lock:
        for old_token, (_, expires_at) in list(_sessions.items()):
            if expires_at <= now:
                del _sessions[old_token]
        _sessions[token] = (username, now + settings.session_lifetime_seconds)
    return token


def session_user(token: str | None) -> str | None:
    with _lock:
        session = _sessions.get(token)
        if session is None:
            return None
        username, expires_at = session
        if expires_at <= monotonic() or username != settings.admin_username:
            _sessions.pop(token, None)
            return None
        return username


def revoke_session(token: str | None) -> None:
    with _lock:
        _sessions.pop(token, None)
