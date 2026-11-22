"""Connect a computer without typing the server key (contract: Connect, C1 to C4).

Setup on a computer calls C1 and C4, with no key and no session. The dashboard calls C2 and
C3 with the admin cookie. The client key goes only to the computer that made the request and
that holds its secret request_id, and only after the admin approved the request.

Every answer of the four endpoints carries Cache-Control: no-store (see no_store_for_connect).
Nothing about a request is logged. The requests live in connect_requests.py.
"""
from fastapi import APIRouter, Depends, HTTPException, Request, Response

from .. import connect_requests
from ..auth import require_admin
from ..config import settings
from ..schemas import (
    ConnectClaimIn,
    ConnectClaimOut,
    ConnectRequestIn,
    ConnectRequestInfo,
    ConnectRequestOut,
)

router = APIRouter(prefix="/api/connect", tags=["connect"])
admin_router = APIRouter(
    prefix="/api/admin/connect", tags=["admin"], dependencies=[Depends(require_admin)]
)

NO_STORE_PREFIXES = ("/api/connect", "/api/admin/connect")
KEY_MISSING = "Client key is not configured"
NOT_FOUND = "Connect request not found"


async def no_store_for_connect(request: Request, call_next) -> Response:
    """Add Cache-Control: no-store to every answer of the Connect endpoints.

    It is a middleware, so the header is also on the errors that no route code makes
    (401 from the session check and 422 from the validation).

    Args:
        request: The request that came in.
        call_next: The next step of the app.

    Returns:
        The answer of the app, with the header added for the Connect paths.
    """
    response = await call_next(request)
    if request.url.path.startswith(NO_STORE_PREFIXES):
        response.headers["Cache-Control"] = "no-store"
    return response


def require_client_key_set() -> None:
    """Refuse to work while the server has no client key to hand out.

    Raises:
        HTTPException: 503, the setting EWS_CLIENT_KEY is empty.
    """
    if not settings.client_key:
        raise HTTPException(status_code=503, detail=KEY_MISSING)


def not_found() -> HTTPException:
    """Make the 404 for an unknown, expired or claimed request.

    Returns:
        The exception, for the caller to raise.
    """
    return HTTPException(status_code=404, detail=NOT_FOUND)


@router.post("/requests", status_code=201, response_model=ConnectRequestOut)
def create_connect_request(body: ConnectRequestIn) -> ConnectRequestOut:
    """C1: a computer asks to be connected.

    Args:
        body: The device id, name, os and device type.

    Returns:
        The secret request_id for the computer, the code for the admin and the lifetime.

    Raises:
        HTTPException: 503 when the server has no client key, 429 when 20 requests wait.
    """
    require_client_key_set()
    request = connect_requests.create_request(body.device_id, body.device_name, body.os, body.device_type)
    if request is None:
        raise HTTPException(status_code=429, detail="Too many connect requests")
    return ConnectRequestOut(
        request_id=request.request_id,
        user_code=request.user_code,
        expires_in_seconds=settings.connect_request_lifetime_seconds,
    )


@admin_router.get("/requests/{user_code}", response_model=ConnectRequestInfo)
def read_connect_request(user_code: str) -> ConnectRequestInfo:
    """C2: the admin reads a request to check the computer and the code.

    Args:
        user_code: The code, in any case, with or without the hyphen.

    Returns:
        The computer and the state of the request. It never holds the request_id.

    Raises:
        HTTPException: 404, the code is malformed, unknown, expired or claimed.
    """
    request = connect_requests.find_by_code(user_code)
    if request is None:
        raise not_found()
    return ConnectRequestInfo(
        user_code=request.user_code,
        device_id=request.device_id,
        device_name=request.device_name,
        os=request.os,
        device_type=request.device_type,
        approved=request.approved,
        expires_in_seconds=request.seconds_left(),
    )


@admin_router.post("/requests/{user_code}/approve", status_code=204)
def approve_connect_request(user_code: str) -> Response:
    """C3: the admin approves a request. Approving twice is fine.

    Args:
        user_code: The code, in any case, with or without the hyphen.

    Returns:
        An empty answer (204).

    Raises:
        HTTPException: 404, the code is malformed, unknown, expired or claimed.
    """
    if not connect_requests.approve(user_code):
        raise not_found()
    return Response(status_code=204)


@router.post("/claim", response_model=ConnectClaimOut, response_model_exclude_none=True)
def claim_connect_request(body: ConnectClaimIn) -> ConnectClaimOut:
    """C4: the computer asks whether the admin approved. The key is given once.

    The key check comes first and consumes nothing, so a request that is approved while the
    key is missing can still be claimed after the key is set again.

    Args:
        body: The secret request_id from C1. It is in the body because addresses are logged.

    Returns:
        pending while the admin has not approved. approved with the client key, once.

    Raises:
        HTTPException: 503 when the server has no client key, 404 when the request is unknown,
            expired or already claimed.
    """
    require_client_key_set()
    request = connect_requests.claim(body.request_id)
    if request is None:
        raise not_found()
    if not request.approved:
        return ConnectClaimOut(status="pending")
    return ConnectClaimOut(status="approved", client_key=settings.client_key)
