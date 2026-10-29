"""Admin login and logout (contract 5.6 and 5.7)."""
import secrets

from fastapi import APIRouter, Depends, HTTPException, Request, Response

from ..auth import require_admin
from ..config import settings
from ..schemas import LoginIn, LoginOut
from ..sessions import create_session, revoke_session

router = APIRouter(prefix="/api", tags=["auth"])


@router.post("/login", response_model=LoginOut)
def login(body: LoginIn, request: Request, response: Response) -> LoginOut:
    username_matches = secrets.compare_digest(
        body.username.encode("utf-8"), settings.admin_username.encode("utf-8")
    )
    password_matches = secrets.compare_digest(
        body.password.get_secret_value().encode("utf-8"), settings.admin_password.encode("utf-8")
    )
    if not settings.admin_username or not settings.admin_password or not (
        username_matches and password_matches
    ):
        raise HTTPException(status_code=401, detail="Wrong username or password")

    # A second login replaces this browser's old token.
    revoke_session(request.cookies.get("ews_session"))
    token = create_session(settings.admin_username)
    response.set_cookie(
        "ews_session", token, max_age=settings.session_lifetime_seconds,
        httponly=True, samesite="lax", secure=request.url.scheme == "https", path="/",
    )
    response.headers["Cache-Control"] = "no-store"
    return LoginOut(username=settings.admin_username)


@router.post("/logout", status_code=204, dependencies=[Depends(require_admin)])
def logout(request: Request) -> Response:
    revoke_session(request.cookies.get("ews_session"))
    response = Response(status_code=204, headers={"Cache-Control": "no-store"})
    response.delete_cookie(
        "ews_session", path="/", httponly=True, samesite="lax",
        secure=request.url.scheme == "https",
    )
    return response
