// Summary endpoint tests. Run with npm test; real-mode fetches use isolated responses.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { getSummary, getDevices, getSettings } from './index.js'
import sample from './summary.json'

/** @returns {void} Installs a fetch spy and resets the summary mode. */
function prepare() {
  vi.stubGlobal('fetch', vi.fn())
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', '')
}

/** @returns {void} Restores request globals and build-time modes. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {Promise<void>} Checks recorded-day semantics and independent copies. */
async function checkSample() {
  const first = await getSummary()
  expect(first).toEqual(sample)
  first.series[0].energy_uj = -1
  expect(await getSummary()).toEqual(sample)
  expect(await getSummary('2026-10-19')).toMatchObject({
    date: '2026-10-19', energy_uj: 0, cost_gbp: 0, co2_kg: 0, series: [],
  })
  expect(fetch).not.toHaveBeenCalled()
}
it('keeps the recorded sample day by default', checkSample)

/** @returns {Promise<void>} Checks the server chooses the current UK day. */
async function checkDefaultDate() {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  fetch.mockResolvedValue({ ok: true, json: async () => sample })
  expect(await getSummary()).toEqual(sample)
  expect(fetch).toHaveBeenCalledWith('/api/summary', { method: 'GET', credentials: 'include' })
}
it('omits an unspecified date in real mode', checkDefaultDate)

/** @returns {Promise<void>} Checks explicit date, credentials and unchanged energy totals. */
async function checkExplicitDate() {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  const response = { ...sample, date: '2026-10-19', energy_uj: 48600000000000,
    cost_gbp: 3.38, co2_kg: 2.79 }
  fetch.mockResolvedValue({ ok: true, json: async () => response })
  expect(await getSummary('2026-10-19')).toEqual(response)
  expect(fetch).toHaveBeenCalledWith('/api/summary?date=2026-10-19', { method: 'GET', credentials: 'include' })
}
it('passes the chosen date without converting API values', checkExplicitDate)

/**
 * Checks empty, partial and clock-change day buckets remain exactly as sent.
 * @param {number} count - Number of half-hour buckets.
 * @param {string} date - UK calendar day.
 * @param {string} start - UTC start of the UK day.
 * @returns {Promise<void>} Resolves after preservation assertions.
 */
async function checkBuckets(count, date, start) {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  const series = Array.from({ length: count }, (_, index) => ({
    timestamp: new Date(Date.parse(start) + index * 1800000).toISOString(), energy_uj: 900000000000,
  }))
  const response = { ...sample, date, series }
  fetch.mockResolvedValue({ ok: true, json: async () => response })
  expect(await getSummary(date)).toEqual(response)
}
it.each([
  [0, '2026-11-11', '2026-11-11T00:00:00Z'],
  [3, '2026-11-11', '2026-11-11T00:00:00Z'],
  [46, '2026-03-29', '2026-03-29T00:00:00Z'],
  [50, '2026-10-25', '2026-10-24T23:00:00Z'],
])('preserves %s buckets for %s', checkBuckets)

/** @param {number} status - HTTP failure. @returns {Promise<void>} Checks status and no sample fallback. */
async function checkHttpError(status) {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  fetch.mockResolvedValue({ ok: false, status })
  await expect(getSummary('invalid')).rejects.toMatchObject({ status })
  expect(fetch).toHaveBeenCalledWith('/api/summary?date=invalid', { method: 'GET', credentials: 'include' })
}
it.each([401, 422, 500])('exposes HTTP %s without sample fallback', checkHttpError)

/** @returns {Promise<void>} Checks network failures stay visible. */
async function checkNetwork() {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  fetch.mockRejectedValue(new TypeError('Network unavailable'))
  await expect(getSummary()).rejects.toThrow('Network unavailable')
}
it('exposes network errors', checkNetwork)

/** @param {*} data - Invalid response. @returns {Promise<void>} Checks malformed data is not an empty success. */
async function checkMalformed(data) {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  fetch.mockResolvedValue({ ok: true, json: async () => data })
  await expect(getSummary()).rejects.toThrow('Invalid summary response')
}
it.each([null, {}, { counts: {}, series: null }, { counts: [], series: [] }])('rejects malformed response %s', checkMalformed)

/** @returns {Promise<void>} Checks JSON parsing errors are exposed. */
async function checkInvalidJson() {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  fetch.mockResolvedValue({ ok: true, json: () => Promise.reject(new SyntaxError('Invalid JSON')) })
  await expect(getSummary()).rejects.toThrow('Invalid JSON')
}
it('rejects invalid JSON', checkInvalidJson)

/** @returns {Promise<void>} Checks the summary mode cannot switch other endpoints. */
async function checkIndependentMode() {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  await getDevices()
  await getSettings()
  expect(fetch).not.toHaveBeenCalled()
}
it('keeps devices and settings in sample mode', checkIndependentMode)
