"""W6 summary fixture: a full UK day of sample buckets (contract 5.10).

The W6 task explicitly asks for a full chart, including future buckets. The W7
live endpoint will omit future buckets and read reports and historical tariffs.
"""
from datetime import date as Date, datetime, time, timedelta, timezone
from decimal import Decimal
from typing import Annotated
from zoneinfo import ZoneInfo

from fastapi import APIRouter, Depends, Query
from fastapi.exceptions import RequestValidationError

from ..auth import require_admin
from ..schemas import EnergyPoint, StateCounts, SummaryOut

router = APIRouter(prefix="/api", tags=["summary"], dependencies=[Depends(require_admin)])
UK = ZoneInfo("Europe/London")


def uk_today() -> Date:
    return datetime.now(UK).date()


def day_buckets(day: Date) -> list[datetime]:
    # Iterate in UTC so the repeated autumn hour and skipped spring hour work.
    start = datetime.combine(day, time.min, UK).astimezone(timezone.utc)
    end = datetime.combine(day + timedelta(days=1), time.min, UK).astimezone(timezone.utc)
    return [start + timedelta(minutes=30 * i) for i in range(int((end - start).total_seconds() // 1800))]


@router.get("/summary", response_model=SummaryOut)
def summary(date: Annotated[str | None, Query(pattern=r"^\d{4}-\d{2}-\d{2}$")] = None) -> SummaryOut:
    try:
        day = Date.fromisoformat(date) if date is not None else uk_today()
        buckets = day_buckets(day)
    except (ValueError, OverflowError) as exc:
        raise RequestValidationError([{
            "type": "value_error", "loc": ("query", "date"),
            "msg": "date must be a supported YYYY-MM-DD calendar date", "input": date,
        }]) from exc
    series = [EnergyPoint(timestamp=start, energy_uj=900_000_000_000) for start in buckets]
    energy = sum(point.energy_uj for point in series)
    kwh = Decimal(energy) / Decimal(3_600_000_000_000)
    return SummaryOut(
        date=day.isoformat(), counts=StateCounts(active=1, idle=1, unused=1, no_data=0, total=3),
        energy_uj=energy, cost_gbp=float(kwh * Decimal("0.25")),
        co2_kg=float(kwh * Decimal("0.207")), price_gbp_per_kwh=0.25,
        co2_kg_per_kwh=0.207, series=series,
    )
