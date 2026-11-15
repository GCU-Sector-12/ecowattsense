// Sample history loading and display helpers used by HistoryPage and its tests.
import { getDeviceHistory, getDevices } from './index.js'
import sampleHistory from './history.json'

export const SAMPLE_HISTORY_FROM = sampleHistory.from
const STATE_NAMES = ['unused', 'idle', 'active']

/**
 * Loads device details and the fixed sample day without changing the shared range provider.
 * The selected UI date deliberately does not filter sample history (EWS_WEB-30).
 * @param {string} deviceId - Requested computer identifier.
 * @returns {Promise<{device: object|null, history: object|null}>} Details and available history.
 * @throws {Error} When either provider fails.
 */
export async function loadComputerHistory(deviceId) {
  const [response, history] = await Promise.all([
    getDevices(), getDeviceHistory(deviceId, sampleHistory.from, sampleHistory.to),
  ])
  const device = response.devices.find(item => item.device_id === deviceId) ?? null
  return { device, history }
}

/**
 * Gives a native date input today's local calendar date, without a UTC day shift.
 * @param {Date} [date] - Local date to format.
 * @returns {string} Calendar date in YYYY-MM-DD form.
 */
export function calendarDay(date = new Date()) {
  const month = String(date.getMonth() + 1).padStart(2, '0')
  const day = String(date.getDate()).padStart(2, '0')
  return `${date.getFullYear()}-${month}-${day}`
}

/**
 * Formats whole elapsed minutes as hours and minutes, including durations over one day.
 * @param {number} seconds - Nonnegative elapsed seconds from the API.
 * @returns {string} Duration such as 10 h 0 min.
 */
export function formatDuration(seconds) {
  const minutes = Math.floor(seconds / 60)
  return `${Math.floor(minutes / 60)} h ${minutes % 60} min`
}

/**
 * Maps the numeric history contract state to the shared badge and colour key.
 * @param {number} state - 0 unused, 1 idle, 2 active.
 * @returns {string} Known state name, or no_data for an unrecognised value.
 */
export function historyState(state) {
  return STATE_NAMES[state] ?? 'no_data'
}

/**
 * Formats report timestamps in UTC so the sample day's blocks retain their stated times.
 * @param {string} timestamp - ISO report timestamp.
 * @returns {string} Time such as 08:30.
 */
export function reportTime(timestamp) {
  return new Intl.DateTimeFormat('en-GB', {
    hour: '2-digit', minute: '2-digit', timeZone: 'UTC',
  }).format(new Date(timestamp))
}

/**
 * Gives the fixed sample day a readable date for the explanation above the reports.
 * @param {string} timestamp - ISO timestamp.
 * @returns {string} Date such as 10 March 2025.
 */
export function historyDate(timestamp) {
  return new Intl.DateTimeFormat('en-GB', {
    day: 'numeric', month: 'long', year: 'numeric', timeZone: 'UTC',
  }).format(new Date(timestamp))
}
