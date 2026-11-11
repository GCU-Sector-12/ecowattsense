// Data providers used by dashboard pages instead of importing JSON in the UI.
// Await the exported functions for fresh API-shaped responses in each endpoint's mode.
import { loadSummary } from './summaryProvider.js'
import devices from './devices.json'
import history from './history.json'
import settings from './settings.json'
import { ujToKwh } from './units.js'

/**
 * Loads the endpoint's selected sample/real summary without converting its units.
 * @param {string} [date] - Requested YYYY-MM-DD; real mode defaults to the server's UK today.
 * @returns {Promise<object>} Summary using API contract field names.
 * @throws {Error} If the real request fails; HTTP failures include status.
 */
export async function getSummary(date) {
  return loadSummary(date)
}

/**
 * Returns an independent copy of the sample device-list response.
 * @returns {Promise<{devices: object[]}>} Devices in API contract form.
 */
export async function getDevices() {
  return structuredClone(devices)
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
