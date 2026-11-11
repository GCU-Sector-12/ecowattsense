// Sample providers used by dashboard pages instead of importing JSON in the UI.
// Await the exported functions for independent copies of API-shaped responses.
import summary from './summary.json'
import { loadDevices } from './devicesProvider.js'
import history from './history.json'
import settings from './settings.json'
import { ujToKwh } from './units.js'

/**
 * Returns an independent sample summary; unrecorded dates have empty energy data.
 * @param {string} [date=summary.date] - Requested YYYY-MM-DD day.
 * @returns {Promise<object>} Summary using API contract field names.
 */
export async function getSummary(date = summary.date) {
  // Only one day is recorded in the sample. Do not relabel its readings as
  // another day; device counts still describe the current sample devices.
  return structuredClone(date === summary.date ? summary : {
    ...summary, date, energy_uj: 0, cost_gbp: 0, co2_kg: 0, series: [],
  })
}

/**
 * Loads devices using the endpoint's sample/real mode without changing page contracts.
 * @param {object} [options] - Optional location_id and include_archived (default true).
 * @returns {Promise<{devices: object[]}>} Devices in API contract form.
 * @throws {Error} If the real request fails; HTTP failures include status.
 */
export async function getDevices(options) {
  return loadDevices(options)
}

/**
 * Returns sample reports in the inclusive-from, exclusive-to range and recomputes totals.
 * @param {string} device_id - Device identifier; unknown devices return null.
 * @param {string} [from=history.from] - Inclusive ISO timestamp.
 * @param {string} [to=history.to] - Exclusive ISO timestamp.
 * @returns {Promise<object|null>} Filtered history or null for an unknown device.
 * @throws {RangeError} If the range is invalid or exceeds 31 days.
 */
export async function getDeviceHistory(device_id, from = history.from, to = history.to) {
  if (device_id !== history.device_id) return null
  const start = Date.parse(from)
  const end = Date.parse(to)
  if (!Number.isFinite(start) || !Number.isFinite(end) || start >= end ||
      end - start > 31 * 24 * 60 * 60 * 1000) {
    throw new RangeError('History range must be valid and no longer than 31 days')
  }
  const result = structuredClone(history)
  result.from = from
  result.to = to
  // The contract includes from and excludes to, comparing instants rather
  // than strings so equivalent ISO timestamps behave the same way.
  result.reports = result.reports.filter(({ timestamp }) => {
    const time = Date.parse(timestamp)
    return time >= start && time < end
  })
  const totals = {
    active_time_seconds: 0, idle_time_seconds: 0, unused_time_seconds: 0,
    energy_uj: 0, cost_gbp: 0, co2_kg: 0,
  }
  const timeFields = ['unused_time_seconds', 'idle_time_seconds', 'active_time_seconds']
  for (const report of result.reports) {
    totals[timeFields[report.state]] += report.interval_seconds
    totals.energy_uj += report.energy_uj
  }
  // All sample reports use the single tariff in settings.json.
  const energy_kwh = ujToKwh(totals.energy_uj)
  totals.cost_gbp = Number((energy_kwh * settings.price_gbp_per_kwh).toFixed(4))
  totals.co2_kg = Number((energy_kwh * settings.co2_kg_per_kwh).toFixed(4))
  result.totals = totals
  return result
}

/**
 * Returns an independent copy of the sample tariff and threshold settings.
 * @returns {Promise<object>} Settings using API contract field names.
 */
export async function getSettings() {
  return structuredClone(settings)
}
