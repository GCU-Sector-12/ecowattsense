"""POST /api/ping (contract 5.5): tell a client if it is registered and which settings_version is current.

The clients call this endpoint every ping_interval_seconds with the header X-EWS-Key. A ping
never creates a computer. registered is true only when the devices table has the computer.
The command list stays empty until the commands table exists.
"""
from typing import Annotated

from fastapi import APIRouter, Depends
from sqlmodel import Session

from ..auth import require_client_key
from ..db import get_session
from ..models import Device
from ..schemas import PingIn, PingOut
from .request_validation import require_json
from .settings import current_settings_version

router = APIRouter(prefix="/api", tags=["ping"], dependencies=[Depends(require_client_key)])


@router.post("/ping", response_model=PingOut, dependencies=[Depends(require_json)])
def ping(body: PingIn, session: Annotated[Session, Depends(get_session)]) -> PingOut:
    """Answer one ping (contract 5.5).

    Args:
        body: The ping: device_id, unsent_reports and the acks.
        session: The database session.

    Returns:
        The current settings_version, registered (true when the computer is in the devices
        table) and an empty command list.
    """
    registered = session.get(Device, body.device_id) is not None
    return PingOut(settings_version=current_settings_version(session), registered=registered, commands=[])
