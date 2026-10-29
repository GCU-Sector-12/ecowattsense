"""Fixed W6 sample data, never presented as database measurements."""
from .schemas import DashboardDevice, HistoryReport, Tariff


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


def demo_tariffs():
    return [
        Tariff(valid_from="2000-01-01T00:00:00Z", price_gbp_per_kwh=.20, co2_kg_per_kwh=.25),
        Tariff(valid_from="2026-10-19T14:00:00Z", price_gbp_per_kwh=.25, co2_kg_per_kwh=.207),
    ]


def demo_history(device_id):
    if device_id not in {row.device_id for row in demo_devices()}:
        return []
    return [HistoryReport(
        timestamp=timestamp, interval_seconds=seconds, state=state, idle_seconds=idle,
        cpu_percent=cpu, screen_locked=state == 0, on_battery=False,
        energy_uj=energy, energy_source="estimated",
    ) for timestamp, seconds, state, idle, cpu, energy in [
        ("2026-10-19T13:55:00Z", 300, 2, 12, 37.5, 9_000_000_000),
        ("2026-10-19T14:00:00Z", 120, 1, 640, 4.2, 2_400_000_000),
        ("2026-10-19T14:05:00Z", 300, 0, 3700, 0.5, 1_500_000_000),
    ]]
