// History endpoint tests. Run with vitest; fetch is stubbed so no real server or account is used.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { getDeviceHistory } from './index.js'
import { isRealHistory } from './historyProvider.js'

const FROM = '2026-11-14T00:00:00Z'
const TO = '2026-11-15T00:00:00Z'
const body = { device_id: 'abc', reports: [], totals: { energy_uj: 0 } }

/** @returns {void} Selects real mode and installs an isolated fetch spy. */
function prepare() {
  vi.stubEnv('VITE_HISTORY_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: true, status: 200, json: async () => body }))
}

/** @returns {void} Restores globals and build-time modes. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {Promise<void>} Checks URL, cookie option and unchanged response. */
async function checkRealRequest() {
  expect(isRealHistory()).toBe(true)
  const result = await getDeviceHistory('abc', FROM, TO)
  expect(result).toEqual(body)
  const query = new URLSearchParams({ from: FROM, to: TO })
  expect(fetch).toHaveBeenCalledWith('/api/devices/abc/history?' + query, { method: 'GET', credentials: 'include' })
}
it('calls the history endpoint with credentials in real mode', checkRealRequest)

/** @returns {Promise<void>} Checks that an unknown device gives null. */
async function checkNotFound() {
  fetch.mockResolvedValue({ ok: false, status: 404, json: async () => ({ detail: 'Device not found' }) })
  expect(await getDeviceHistory('abc', FROM, TO)).toBeNull()
}
it('returns null for HTTP 404', checkNotFound)

/** @param {number} status - Failing HTTP status. @returns {Promise<void>} Checks the error keeps its status. */
async function checkHttpError(status) {
  fetch.mockResolvedValue({ ok: false, status, json: async () => ({}) })
  await expect(getDeviceHistory('abc', FROM, TO)).rejects.toMatchObject({ status })
}
it.each([401, 422, 500])('exposes HTTP %s without sample fallback', checkHttpError)

/** @returns {Promise<void>} Checks malformed bodies are errors, not empty days. */
async function checkInvalid() {
  fetch.mockResolvedValue({ ok: true, status: 200, json: async () => ({ device_id: 'abc' }) })
  await expect(getDeviceHistory('abc', FROM, TO)).rejects.toThrow('Invalid history response')
}
it('rejects a malformed response', checkInvalid)

/** @returns {Promise<void>} Checks a failed network request stays visible. */
async function checkNetwork() {
  fetch.mockRejectedValue(new TypeError('network down'))
  await expect(getDeviceHistory('abc', FROM, TO)).rejects.toThrow('network down')
}
it('exposes network failures', checkNetwork)

/** @returns {Promise<void>} Checks sample mode does not call the network. */
async function checkSampleMode() {
  vi.stubEnv('VITE_HISTORY_DATA_SOURCE', '')
  expect(isRealHistory()).toBe(false)
  await getDeviceHistory('unknown-id')
  expect(fetch).not.toHaveBeenCalled()
}
it('keeps sample mode off the network', checkSampleMode)
