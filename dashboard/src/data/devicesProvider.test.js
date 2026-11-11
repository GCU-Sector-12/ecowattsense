// Devices endpoint tests. Run with npm test; fetch is isolated from real accounts.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { getDevices, getSummary, getSettings } from './index.js'
import sample from './devices.json'

/** @returns {void} Installs an isolated fetch spy and selects sample mode. */
function prepare() {
  vi.stubGlobal('fetch', vi.fn())
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', '')
}

/** @returns {void} Restores globals and build-time modes. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {Promise<void>} Checks default sample mode, cloning and no network traffic. */
async function checkSample() {
  const first = await getDevices()
  expect(first).toEqual(sample)
  first.devices[0].display_name = 'Changed'
  expect(await getDevices()).toEqual(sample)
  expect(fetch).not.toHaveBeenCalled()
}
it('keeps sample data as the default', checkSample)

/** @returns {Promise<void>} Checks room filtering in sample mode. */
async function checkSampleRoom() {
  const location_id = sample.devices[0].location_id
  const result = await getDevices({ location_id, include_archived: false })
  expect(result.devices).toEqual(sample.devices.filter(device => device.location_id === location_id && !device.archived))
}
it('supports device filters in sample mode', checkSampleRoom)

/** @returns {Promise<void>} Checks cookies, request options and untouched nullable/state fields. */
async function checkReal() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  const device = { ...sample.devices[0], display_name: null, location_id: null,
    location_name: null, building: null, state: null, state_name: 'no_data', archived: true }
  fetch.mockResolvedValue({ ok: true, json: async () => ({ devices: [device] }) })
  expect(await getDevices()).toEqual({ devices: [device] })
  expect(fetch).toHaveBeenCalledWith('/api/devices?include_archived=true', { method: 'GET', credentials: 'include' })
}
it('loads the real contract without converting nulls or state words', checkReal)

/** @returns {Promise<void>} Checks explicit room and archive parameters and an empty 200. */
async function checkQuery() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  fetch.mockResolvedValue({ ok: true, json: async () => ({ devices: [] }) })
  expect(await getDevices({ location_id: 1, include_archived: false })).toEqual({ devices: [] })
  expect(fetch).toHaveBeenCalledWith('/api/devices?include_archived=false&location_id=1', { method: 'GET', credentials: 'include' })
}
it('sends query filters and accepts an empty response', checkQuery)

/** @param {number} status - HTTP failure. @returns {Promise<void>} Checks errors cannot become sample success. */
async function checkHttpError(status) {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  fetch.mockResolvedValue({ ok: false, status })
  await expect(getDevices()).rejects.toMatchObject({ status })
}
it.each([401, 422, 500])('exposes HTTP %s without sample fallback', checkHttpError)

/** @returns {Promise<void>} Checks transport failure propagation. */
async function checkNetworkError() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  fetch.mockRejectedValue(new TypeError('Network unavailable'))
  await expect(getDevices()).rejects.toThrow('Network unavailable')
}
it('exposes network failures', checkNetworkError)

/** @param {*} data - Invalid body. @returns {Promise<void>} Checks response shape validation. */
async function checkMalformed(data) {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  fetch.mockResolvedValue({ ok: true, json: async () => data })
  await expect(getDevices()).rejects.toThrow('Invalid devices response')
}
it.each([null, {}, { devices: null }])('rejects malformed response %s', checkMalformed)

/** @returns {Promise<void>} Checks JSON parsing failure propagation. */
async function checkInvalidJson() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  fetch.mockResolvedValue({ ok: true, json: () => Promise.reject(new SyntaxError('Invalid JSON')) })
  await expect(getDevices()).rejects.toThrow('Invalid JSON')
}
it('rejects invalid JSON', checkInvalidJson)

/** @returns {Promise<void>} Checks that enabling devices does not enable other endpoints. */
async function checkIndependentMode() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  await getSummary()
  await getSettings()
  expect(fetch).not.toHaveBeenCalled()
}
it('keeps other endpoints in sample mode', checkIndependentMode)
