"""Device registration. W6 stub: acknowledges the name without storing it."""
from uuid import UUID

from fastapi import APIRouter, Depends

from ..auth import require_client_key
from ..schemas import DeviceIn, DeviceOut
from .request_validation import require_json

router = APIRouter(prefix="/api", tags=["devices"])


@router.put("/devices/{device_id}", response_model=DeviceOut,
            dependencies=[Depends(require_client_key), Depends(require_json)])
def register_device(device_id: UUID, body: DeviceIn) -> DeviceOut:
    return DeviceOut(device_id=device_id, device_name=body.device_name, created=True)
