"""Authentication dependencies shared by the API routers."""
import secrets
from typing import Annotated

from fastapi import Cookie, Header, HTTPException

from .config import settings
from .sessions import session_user


def require_client_key(
    key: Annotated[str | None, Header(alias="X-EWS-Key")] = None,
) -> None:
    """Reject missing, incorrect and unconfigured client keys."""
    if not settings.client_key or key is None or not secrets.compare_digest(
        key.encode("utf-8"), settings.client_key.encode("utf-8")
    ):
        raise HTTPException(status_code=401, detail="Invalid or missing client key")


def require_admin(
    token: Annotated[str | None, Cookie(alias="ews_session")] = None,
) -> str:
    username = session_user(token)
    if username is None:
        raise HTTPException(status_code=401, detail="Invalid or missing session")
    return username
