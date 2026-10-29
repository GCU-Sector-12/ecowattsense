"""Dashboard totals use the tariff at each report timestamp, never today's price."""
from bisect import bisect_right
from decimal import Decimal

from .schemas import HistoryReport, HistoryTotals, Tariff

MICROJOULES_PER_KWH = Decimal(3_600_000_000_000)


def report_totals(reports: list[HistoryReport], tariffs: list[Tariff]) -> HistoryTotals:
    tariffs = sorted(tariffs, key=lambda tariff: tariff.valid_from)
    starts = [tariff.valid_from for tariff in tariffs]
    totals = HistoryTotals()
    cost = Decimal(0)
    co2 = Decimal(0)
    for report in reports:
        index = bisect_right(starts, report.timestamp) - 1
        if index < 0:
            raise ValueError("No tariff covers the report timestamp")
        tariff = tariffs[index]
        kwh = Decimal(report.energy_uj) / MICROJOULES_PER_KWH
        cost += kwh * Decimal(str(tariff.price_gbp_per_kwh))
        co2 += kwh * Decimal(str(tariff.co2_kg_per_kwh))
        totals.energy_uj += report.energy_uj
        if report.state == 2:
            totals.active_time_seconds += report.interval_seconds
        elif report.state == 1:
            totals.idle_time_seconds += report.interval_seconds
        else:
            totals.unused_time_seconds += report.interval_seconds
    # Round only when presenting; do not round every report's small cost to pence.
    totals.cost_gbp = float(cost)
    totals.co2_kg = float(co2)
    return totals
