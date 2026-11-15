// Sample history loading and display helpers used by HistoryPage and its tests.
import { getDeviceHistory, getDevices } from './index.js'
import { isRealHistory } from './historyProvider.js'
import sampleHistory from './history.json'

export const SAMPLE_HISTORY_FROM = sampleHistory.from
const STATE_NAMES = ['unused', 'idle', 'active']

/**
 * Loads device details and one computer's history. Sample mode returns the fixed sample day
 * whatever the date (EWS_WEB-30); real mode loads the selected UTC day from the API.
 * @param {string} deviceId - Requested computer identifier.
 * @param {string} [day] - YYYY-MM-DD day to load in real mode.
 * @returns {Promise<{device: object|null, history: object|null}>} Details and available history.
 *   In real mode a 404 from the history endpoint gives device null.
 * @throws {Error} When either provider fails.
 */
export async function loadComputerHistory(deviceId, day) {
  const range = isRealHistory() ? dayRange(day) : { from: sampleHistory.from, to: sampleHistory.to }
  const [response, history] = await Promise.all([
    getDevices(), getDeviceHistory(deviceId, range.from, range.to),
  ])
  const found = response.devices.find(item => item.device_id === deviceId) ?? null
  const device = isRealHistory() && !history ? null : found
  return { device, history }
}

/**
 * Turns a calendar day into the UTC range the history endpoint expects.
 * @param {string} day - YYYY-MM-DD.
 * @returns {{from: string, to: string}} 00:00Z of the day and 00:00Z of the next day (exclusive).
 */
export function dayRange(day) {
  const start = Date.parse(day + 'T00:00:00Z')
  const next = new Date(start + 24 * 60 * 60 * 1000)
  return { from: day + 'T00:00:00Z', to: next.toISOString().slice(0, 19) + 'Z' }
}

/**
 * Sums report energy into half-hour UTC buckets, in the shape EnergyChart takes.
 * Buckets without reports are left out. Reports on battery carry 0 and add 0.
 * @param {object[]} reports - History reports with timestamp and energy_uj.
 * @returns {{timestamp: string, energy_uj: number}[]} Buckets in time order.
 */
export function energyByHalfHour(reports) {
  const sums = new Map()
  for (const report of reports) addToBucket(sums, report)
  return [...sums.entries()].sort(([a], [b]) => a - b).map(toBucket)
}

/**
 * Adds one report to its half-hour bucket.
 * @param {Map<number, number>} sums - Bucket start in epoch milliseconds to energy_uj.
 * @param {object} report - Report with timestamp and energy_uj.
 * @returns {void}
 */
function addToBucket(sums, report) {
  const half = 30 * 60 * 1000
  const start = Math.floor(Date.parse(report.timestamp) / half) * half
  sums.set(start, (sums.get(start) ?? 0) + report.energy_uj)
}

/**
 * Converts one bucket entry to a chart point.
 * @param {[number, number]} entry - Bucket start in epoch milliseconds and its energy_uj.
 * @returns {{timestamp: string, energy_uj: number}} Chart point with a UTC timestamp.
 */
function toBucket([start, energy_uj]) {
  return { timestamp: new Date(start).toISOString().slice(0, 19) + 'Z', energy_uj }
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

/**
 * Moves a calendar day by whole days.
 * @param {string} day - YYYY-MM-DD.
 * @param {number} delta - Days to add; negative goes back.
 * @returns {string} The new YYYY-MM-DD day.
 */
export function shiftDay(day, delta) {
  const moved = new Date(Date.parse(day + 'T00:00:00Z') + delta * 24 * 60 * 60 * 1000)
  return moved.toISOString().slice(0, 10)
}

/**
 * Gives a selected UTC day a short readable form for the chart title.
 * @param {string} day - YYYY-MM-DD.
 * @returns {string} Date such as 14 Nov 2026.
 */
export function shortDay(day) {
  return new Intl.DateTimeFormat('en-GB', {
    day: 'numeric', month: 'short', year: 'numeric', timeZone: 'UTC',
  }).format(new Date(day + 'T00:00:00Z'))
}

/**
 * Places a report inside its UTC day for the timeline.
 * @param {object} report - Report with timestamp and interval_seconds.
 * @returns {{left: string, width: string}} CSS percentages of the 24 hour day.
 */
export function timelinePlace(report) {
  const time = new Date(report.timestamp)
  const minutes = time.getUTCHours() * 60 + time.getUTCMinutes() + time.getUTCSeconds() / 60
  return {
    left: (minutes / 1440 * 100) + '%',
    width: (report.interval_seconds / 86400 * 100) + '%',
  }
}

/**
 * Describes which rows of the table are on screen.
 * @param {number} page - 1-based page.
 * @param {number} pageSize - Rows per page.
 * @param {number} total - Number of reports.
 * @returns {string} Text such as Showing 1 to 25 of 60 reports.
 */
export function rangeText(page, pageSize, total) {
  const first = total ? (page - 1) * pageSize + 1 : 0
  const last = Math.min(page * pageSize, total)
  return `Showing ${first} to ${last} of ${total} reports`
}
