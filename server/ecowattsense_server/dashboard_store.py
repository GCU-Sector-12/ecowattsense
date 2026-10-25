"""Read the agreed database tables without owning their models or migrations.

EWS_API-6 and the W7 database tasks own schema creation. Reflection lets these
read endpoints use their actual tables, including SQLModel's SQLite UUID type.
Database mode fails explicitly when those tables or tariff seeds are missing.
"""
from datetime import date, datetime, time, timedelta, timezone
from uuid import UUID
from zoneinfo import ZoneInfo

from fastapi import HTTPException
from sqlalchemy import MetaData, Table, inspect, select
from sqlmodel import Session

from .config import settings
from .dashboard_totals import report_totals
from .schemas import DashboardDevice, EnergyPoint, HistoryOut, HistoryReport, StateCounts, SummaryOut, Tariff

UK = ZoneInfo("Europe/London")


def utc(value) -> datetime:
    if isinstance(value, str):
        value = datetime.fromisoformat(value)
    if value.tzinfo is None:
        value = value.replace(tzinfo=timezone.utc)
    return value.astimezone(timezone.utc)


def tables(session: Session, *names: str) -> dict[str, Table]:
    connection = session.connection()
    inspector = inspect(connection)
    if any(not inspector.has_table(name) for name in names):
        raise HTTPException(503, "Dashboard database tables are not ready")
    metadata = MetaData()
    return {name: Table(name, metadata, autoload_with=connection, resolve_fks=False) for name in names}


def report_model(row) -> HistoryReport:
    return HistoryReport(**{**dict(row), "timestamp": utc(row["timestamp"])})


def tariff_rows(session: Session) -> list[Tariff]:
    tariff = tables(session, "tariffs")["tariffs"]
    rows = session.execute(select(tariff).order_by(tariff.c.valid_from)).mappings()
    return [Tariff(**{**dict(row), "valid_from": utc(row["valid_from"])}) for row in rows]


def totals_or_unavailable(reports, tariffs):
    try:
        return report_totals(reports, tariffs)
    except ValueError as exc:
        raise HTTPException(503, "No tariff covers the requested reports") from exc


def devices_from_database(session: Session, now: datetime) -> list[DashboardDevice]:
    schema = tables(session, "devices", "reports", "locations")
    device, report, location = (schema[name] for name in ("devices", "reports", "locations"))
    locations = {row["id"]: row for row in session.execute(select(location)).mappings()}
    # Select only the newest report per device. Late uploads cannot replace its state.
    latest = select(report.c.timestamp).where(report.c.device_id == device.c.id).order_by(
        report.c.timestamp.desc()).limit(1).correlate(device).scalar_subquery()
    query = select(device, report.c.state, report.c.timestamp.label("report_timestamp")).outerjoin(
        report, (report.c.device_id == device.c.id) & (report.c.timestamp == latest))
    result = []
    for row in session.execute(query).mappings():
        room = locations.get(row["location_id"])
        timestamp = row["report_timestamp"]
        stale = timestamp is None or utc(timestamp) <= now - timedelta(
            seconds=settings.no_data_after_cycles * settings.report_interval_seconds)
        state = row["state"]
        result.append(DashboardDevice(
            device_id=UUID(str(row["id"])), display_name=row["display_name"],
            location_id=row["location_id"], location_name=room["name"] if room else None,
            building=room["building"] if room else None, os=row["os"], device_type=row["device_type"],
            state=state, state_name="no_data" if stale else {2: "active", 1: "idle", 0: "unused"}[state],
            first_seen_at=utc(row["first_seen_at"]), last_seen_at=utc(row["last_seen_at"]),
            archived=row["archived"],
        ))
    return result


def history_from_database(session: Session, device_id: UUID, start: datetime, end: datetime) -> HistoryOut:
    schema = tables(session, "devices", "reports")
    device, report = schema["devices"], schema["reports"]
    # UUID is stored as 32 hex characters by SQLModel; accept a hyphenated column too.
    row = session.execute(select(device).where(device.c.id.in_([device_id.hex, str(device_id)]))).mappings().first()
    if row is None:
        raise HTTPException(404, "Device not found")
    query = select(report).where(
        report.c.device_id == row["id"],
        report.c.timestamp >= start.replace(tzinfo=None),
        report.c.timestamp < end.replace(tzinfo=None),
    ).order_by(report.c.timestamp)
    reports = [report_model(item) for item in session.execute(query).mappings()]
    totals = totals_or_unavailable(reports, tariff_rows(session))
    return HistoryOut(device_id=device_id, display_name=row["display_name"],
                      from_time=start, to_time=end, reports=reports, totals=totals)


def summary_from_database(session: Session, day: date, now: datetime) -> SummaryOut:
    start = datetime.combine(day, time.min, UK).astimezone(timezone.utc)
    end = datetime.combine(day + timedelta(days=1), time.min, UK).astimezone(timezone.utc)
    report = tables(session, "reports")["reports"]
    query = select(report).where(report.c.timestamp >= start.replace(tzinfo=None),
                                report.c.timestamp < min(end, now).replace(tzinfo=None))
    reports = [report_model(row) for row in session.execute(query).mappings()]
    tariffs = tariff_rows(session)
    current = [tariff for tariff in tariffs if tariff.valid_from <= now]
    if not current:
        raise HTTPException(503, "No current tariff is configured")
    totals = totals_or_unavailable(reports, tariffs)
    current_devices = [device for device in devices_from_database(session, now) if not device.archived]
    counts = StateCounts(active=0, idle=0, unused=0, no_data=0, total=len(current_devices))
    for device in current_devices:
        setattr(counts, device.state_name, getattr(counts, device.state_name) + 1)
    series = []
    bucket = start
    while bucket < min(end, now):
        series.append(EnergyPoint(timestamp=bucket, energy_uj=0))
        bucket += timedelta(minutes=30)
    for row in reports:
        index = int((row.timestamp - start).total_seconds() // 1800)
        series[index].energy_uj += row.energy_uj
    return SummaryOut(
        date=day.isoformat(), counts=counts, energy_uj=totals.energy_uj,
        cost_gbp=totals.cost_gbp, co2_kg=totals.co2_kg,
        price_gbp_per_kwh=current[-1].price_gbp_per_kwh,
        co2_kg_per_kwh=current[-1].co2_kg_per_kwh, series=series,
    )
