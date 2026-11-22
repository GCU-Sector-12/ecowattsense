# ecowattsense_server/connect_requests.py
"""Memory store of the pending Connect requests (Connect a computer without typing the server key).

Who uses it: api/connect.py. A computer asks for the client key (C1) and gets a secret
request_id and a short user_code. The admin finds the request by the user_code, checks it
(C2) and approves it (C3). The computer then collects the key with its request_id (C4).

The store is one dict and one Lock, like sessions.py. A restart of the server drops all
requests, which is fine: the computer asks again. The functions here never log anything,
because a request_id, a user_code and the client key are secrets.

The lock holds only calls to small helpers, so no control statement sits inside another one.
"""
import secrets
from dataclasses import dataclass, replace
from math import ceil
from threading import Lock
from time import monotonic
from uuid import UUID

from .config import settings

# At most this many requests wait at the same time (not expired, not claimed).
MAX_PENDING = 20
# No I, O, 0 or 1, so a code read aloud or from a screen is not mixed up.
CODE_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
CODE_LENGTH = 8

_requests: dict[str, "ConnectRequest"] = {}
_lock = Lock()


def now() -> float:
    """Give the clock of the store, in seconds.

    It is a function of its own, so the tests can move the time without sleeping.

    Returns:
        A time that never goes back (time.monotonic).
    """
    return monotonic()


@dataclass(frozen=True)
class ConnectRequest:
    """One request of a computer. It is never changed: approving makes a new copy."""

    request_id: str
    code: str
    device_id: UUID
    device_name: str | None
    os: str
    device_type: str
    approved: bool
    expires_at: float

    @property
    def user_code(self) -> str:
        """The code as people read it: four characters, a hyphen, four characters."""
        return f"{self.code[:4]}-{self.code[4:]}"

    def is_expired(self) -> bool:
        """Say whether the lifetime of the request is over.

        Returns:
            True when the request must be treated as unknown.
        """
        return self.expires_at <= now()

    def seconds_left(self) -> int:
        """Count the seconds until the request expires.

        Returns:
            Whole seconds, rounded up, never below 0.
        """
        return max(0, ceil(self.expires_at - now()))


def normalize_code(text: str) -> str | None:
    """Turn what the admin typed into the stored form of a code.

    Case and hyphens do not matter: abcd-efgh, ABCD-EFGH and ABCDEFGH are one code.

    Args:
        text: The code as it came from the address.

    Returns:
        The code in capitals without hyphens, or None when it is not 8 allowed characters.
    """
    code = text.upper().replace("-", "")
    if len(code) != CODE_LENGTH:
        return None
    if not set(code) <= set(CODE_ALPHABET):
        return None
    return code


def _random_code() -> str:
    """Make one random code of 8 allowed characters with secrets.choice.

    Returns:
        The code without a hyphen.
    """
    return "".join(secrets.choice(CODE_ALPHABET) for _ in range(CODE_LENGTH))


def _new_code() -> str:
    """Make a code that no pending request uses. The caller holds the lock.

    Returns:
        The code without a hyphen.
    """
    taken = {request.code for request in _requests.values()}
    code = _random_code()
    while code in taken:
        code = _random_code()
    return code


def _remove_expired() -> None:
    """Delete the requests whose lifetime is over. The caller holds the lock."""
    expired = [request_id for request_id, request in _requests.items() if request.is_expired()]
    for request_id in expired:
        del _requests[request_id]


def _find_live(code: str) -> ConnectRequest | None:
    """Find a request that is not expired by its normalized code. The caller holds the lock.

    Args:
        code: A code from normalize_code.

    Returns:
        The request, or None.
    """
    return next(
        (request for request in _requests.values() if request.code == code and not request.is_expired()),
        None,
    )


def _create_locked(device_id: UUID, device_name: str | None, os: str, device_type: str) -> ConnectRequest | None:
    """Do the work of create_request. The caller holds the lock."""
    _remove_expired()
    if len(_requests) >= MAX_PENDING:
        return None
    request = ConnectRequest(
        request_id=secrets.token_urlsafe(32),
        code=_new_code(),
        device_id=device_id,
        device_name=device_name,
        os=os,
        device_type=device_type,
        approved=False,
        expires_at=now() + settings.connect_request_lifetime_seconds,
    )
    _requests[request.request_id] = request
    return request


def _approve_locked(code: str) -> bool:
    """Do the work of approve. The caller holds the lock."""
    request = _find_live(code)
    if request is None:
        return False
    _requests[request.request_id] = replace(request, approved=True)
    return True


def _claim_locked(request_id: str) -> ConnectRequest | None:
    """Do the work of claim. The caller holds the lock."""
    request = _requests.get(request_id)
    if request is None:
        return None
    if request.is_expired():
        del _requests[request_id]
        return None
    if request.approved:
        del _requests[request_id]
    return request


def create_request(device_id: UUID, device_name: str | None, os: str, device_type: str) -> ConnectRequest | None:
    """Store a new request. Expired requests are removed first.

    Args:
        device_id: The id of the computer that asks.
        device_name: The name the admin typed in Setup, or None.
        os: windows, linux or macos.
        device_type: The type of the computer, for example laptop.

    Returns:
        The new request, or None when MAX_PENDING requests are waiting already.
    """
    with _lock:
        return _create_locked(device_id, device_name, os, device_type)


def find_by_code(user_code: str) -> ConnectRequest | None:
    """Find a waiting request for the admin.

    Args:
        user_code: The code from the address, in any case, with or without the hyphen.

    Returns:
        The request, or None when the code is malformed, unknown, expired or claimed.
    """
    code = normalize_code(user_code)
    if code is None:
        return None
    with _lock:
        return _find_live(code)


def approve(user_code: str) -> bool:
    """Mark a request as approved. Approving again changes nothing.

    Args:
        user_code: The code from the address, in any case, with or without the hyphen.

    Returns:
        True when the request exists, False when it is malformed, unknown, expired or claimed.
    """
    code = normalize_code(user_code)
    if code is None:
        return False
    with _lock:
        return _approve_locked(code)


def claim(request_id: str) -> ConnectRequest | None:
    """Look at a request for the computer that made it. An approved request is removed here.

    The request_id is a random 256 bit secret, so a plain dict lookup is safe.

    Args:
        request_id: The secret from C1.

    Returns:
        The request. When it is approved it is already removed, so a second call gives None.
        None also for an unknown or expired request.
    """
    with _lock:
        return _claim_locked(request_id)
