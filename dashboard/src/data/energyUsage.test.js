// Energy Usage range and totals tests. Run with npm test.
import { expect, it } from 'vitest'
import { buildEnergyRows, formatEnergyCost, getEnergyTotals, getRangeDates } from './energyUsage.js'

/** @returns {void} Checks inclusive date ranges and their order. */
function checkDateRanges() {
  expect(getRangeDates(7, '2025-03-10')).toEqual([
    '2025-03-04', '2025-03-05', '2025-03-06', '2025-03-07',
    '2025-03-08', '2025-03-09', '2025-03-10',
  ])
  expect(getRangeDates(30, '2025-03-10')).toHaveLength(30)
}
it('builds 7-day and 30-day date ranges', checkDateRanges)

/** @returns {void} Checks invalid range and date inputs fail clearly. */
function checkInvalidRanges() {
  expect(() => getRangeDates(14, '2025-03-10')).toThrow(RangeError)
  expect(() => getRangeDates(7, 'not-a-date')).toThrow(RangeError)
}
it('rejects unsupported ranges and invalid end dates', checkInvalidRanges)

/** @returns {void} Checks daily values and range totals use the API energy values. */
function checkTotals() {
  const rows = buildEnergyRows([
    { date: '2025-03-09', energy_uj: 3600000000000, cost_gbp: 0.25, co2_kg: 0.207 },
    { date: '2025-03-10', energy_uj: 7200000000000, cost_gbp: 0.5, co2_kg: 0.414 },
  ])
  expect(rows.map((row) => row.energy_kwh)).toEqual([1, 2])
  expect(getEnergyTotals(rows)).toEqual({ energy_kwh: 3, cost_gbp: 0.75, co2_kg: 0.621 })
  expect(formatEnergyCost(0.75)).toBe('£0.75')
}
it('converts daily summaries and totals energy, cost and CO2', checkTotals)
