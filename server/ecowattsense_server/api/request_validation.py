"""Request rules shared by the client JSON endpoints."""
from fastapi import Request
from fastapi.exceptions import RequestValidationError


def require_json(request: Request) -> None:
    media_type = request.headers.get("content-type", "").split(";", 1)[0].strip().lower()
    if media_type != "application/json":
        raise RequestValidationError([{
            "type": "value_error", "loc": ("header", "content-type"),
            "msg": "Content-Type must be application/json", "input": media_type,
        }])
