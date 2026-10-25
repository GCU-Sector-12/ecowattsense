"""Device registration. W6 stub: acknowledges the name without storing it."""
from uuid import UUID

from fastapi import APIRouter, Depends

from ..auth import require_admin, require_client_key
from ..demo_data import demo_devices
from ..schemas import DeviceIn, DeviceOut, DevicesOut
from .request_validation import require_json

router = APIRouter(prefix="/api", tags=["devices"])


@router.put("/devices/{device_id}", response_model=DeviceOut,
            dependencies=[Depends(require_client_key), Depends(require_json)])
def register_device(device_id: UUID, body: DeviceIn) -> DeviceOut:
    return DeviceOut(device_id=device_id, device_name=body.device_name, created=True)


@router.get("/devices", response_model=DevicesOut, dependencies=[Depends(require_admin)])
def list_devices(location_id: int | None = None, include_archived: bool = False) -> DevicesOut:
    devices = [
        device for device in demo_devices()
        if (location_id is None or device.location_id == location_id)
        and (include_archived or not device.archived)
    ]
    devices.sort(key=lambda device: (device.display_name or "", str(device.device_id)))
    return DevicesOut(devices=devices)
