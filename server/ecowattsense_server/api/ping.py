"""POST /api/ping: W6 stub, no registration or command storage."""
from fastapi import APIRouter, Depends

from ..auth import require_client_key
from ..schemas import PingIn, PingOut
from .request_validation import require_json

router = APIRouter(prefix="/api", tags=["ping"], dependencies=[Depends(require_client_key)])


@router.post("/ping", response_model=PingOut, dependencies=[Depends(require_json)])
def ping(body: PingIn) -> PingOut:
    return PingOut(settings_version=1, registered=True, commands=[])
