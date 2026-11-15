// Date ranges and totals for the Energy Usage page. Used by EnergyPage and its tests.
import { ujToKwh, formatGbp } from './units.js'

const dayFormatter = new Intl.DateTimeFormat('en-GB', {
  day: '2-digit', month: 'short', timeZone: 'UTC',
})

/**
 * Builds the calendar dates for a 7-day or 30-day view, including the end date.
 * @param {number} range - Number of days to include.
 * @param {string} endDate - Last date as YYYY-MM-DD.
 * @returns {string[]} Dates in oldest-to-newest order.
 * @throws {RangeError} When the range is not 7 or 30 days, or the end date is invalid.
 */
export function getRangeDates(range, endDate) {
  if (![7, 30].includes(range)) throw new RangeError('Energy range must be 7 or 30 days')
  const end = Date.parse(endDate + 'T00:00:00Z')
  if (!Number.isFinite(end)) throw new RangeError('Energy end date must be YYYY-MM-DD')
  return Array.from({ length: range }, (_, index) =>
    new Date(end - (range - index - 1) * 86400000).toISOString().slice(0, 10))
}

/**
 * Converts daily API summaries into rows suitable for the chart and table.
 * @param {object[]} summaries - Daily summaries returned by getSummary.
 * @returns {object[]} Rows with date, label, energy_kwh, cost_gbp and co2_kg.
 */
export function buildEnergyRows(summaries) {
  return summaries.map((summary) => ({
    date: summary.date,
    label: dayFormatter.format(new Date(summary.date + 'T00:00:00Z')),
    energy_kwh: ujToKwh(summary.energy_uj),
    cost_gbp: summary.cost_gbp,
    co2_kg: summary.co2_kg,
  }))
}

/**
 * Adds daily energy, cost and carbon totals without rounding intermediate values.
 * @param {object[]} rows - Daily Energy Usage rows.
 * @returns {object} Range totals in kWh, GBP and kg CO2.
 */
export function getEnergyTotals(rows) {
  return rows.reduce((totals, row) => ({
    energy_kwh: totals.energy_kwh + row.energy_kwh,
    cost_gbp: totals.cost_gbp + row.cost_gbp,
    co2_kg: totals.co2_kg + row.co2_kg,
  }), { energy_kwh: 0, cost_gbp: 0, co2_kg: 0 })
}

/**
 * Formats a GBP amount using the dashboard's shared currency formatter.
 * @param {number} value - Amount in GBP.
 * @returns {string} Currency amount.
 */
export function formatEnergyCost(value) {
  return formatGbp(value)
}
