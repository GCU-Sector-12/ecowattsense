"""Fixed W6 sample data, never presented as database measurements."""
from .schemas import DashboardDevice


def demo_devices() -> list[DashboardDevice]:
    devices = []
    for device_id, name, state, state_name, location_id in [
        ("6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12", "Lab 1 PC 03", 2, "active", 1),
        ("6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a13", "Lab 1 PC 04", 1, "idle", 1),
        ("6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a14", "Lab 2 PC 01", 0, "unused", 2),
    ]:
        devices.append(DashboardDevice(
            device_id=device_id, display_name=name, location_id=location_id,
            location_name=f"Lab {location_id}", building="George Moore",
            os="windows", device_type="laptop", state=state, state_name=state_name,
            last_seen_at="2026-10-19T14:05:00Z", first_seen_at="2026-10-12T09:00:00Z",
            archived=False,
        ))
    return devices
