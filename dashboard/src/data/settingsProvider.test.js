// Settings provider tests. Run with vitest; fetch is stubbed so no real server or account is used.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { getSettings, saveSettings } from './index.js'
import { isRealSettings } from './settingsProvider.js'
import sample from './settings.json'

const body = { ...sample, settings_version: 4, report_interval_seconds: 120 }

/** @param {number} status - HTTP status. @param {*} json - Parsed body. @returns {object} Fetch answer. */
function answer(status, json) {
  return { ok: status >= 200 && status < 300, status, json: async () => json }
}

/** @returns {void} Selects real mode and installs an isolated fetch spy. */
function prepare() {
  vi.stubEnv('VITE_SETTINGS_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue(answer(200, body)))
}

/** @returns {void} Restores globals and build-time modes. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {Promise<void>} Checks URL, cookie option and unchanged response. */
async function checkRealGet() {
  expect(isRealSettings()).toBe(true)
  expect(await getSettings()).toEqual(body)
  expect(fetch).toHaveBeenCalledWith('/api/admin/settings', { method: 'GET', credentials: 'include' })
}
it('reads GET /api/admin/settings with credentials in real mode', checkRealGet)

/** @returns {Promise<void>} Checks method, headers and body of a save. */
async function checkRealPut() {
  const saved = { ...body, settings_version: 5 }
  fetch.mockResolvedValue(answer(200, saved))
  expect(await saveSettings(body)).toEqual(saved)
  expect(fetch).toHaveBeenCalledWith('/api/admin/settings', {
    method: 'PUT', credentials: 'include',
    headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body),
  })
}
it('saves with PUT, JSON content type and credentials', checkRealPut)

/** @param {number} status - Failing HTTP status. @returns {Promise<void>} Checks the status is kept. */
async function checkLoadError(status) {
  fetch.mockResolvedValue(answer(status, {}))
  await expect(getSettings()).rejects.toMatchObject({ status })
}
it.each([401, 500])('exposes HTTP %s of a load without sample fallback', checkLoadError)

/** @returns {Promise<void>} Checks partial answers are errors, not settings. */
async function checkInvalid() {
  fetch.mockResolvedValue(answer(200, { settings_version: 1 }))
  await expect(getSettings()).rejects.toThrow('Invalid settings response')
}
it('rejects a partial GET answer', checkInvalid)

/** @returns {Promise<void>} Checks a string detail of a 422 becomes the error message. */
async function check422Text() {
  fetch.mockResolvedValue(answer(422, { detail: 'Unused must be greater than idle' }))
  await expect(saveSettings(body)).rejects.toMatchObject({ status: 422, message: 'Unused must be greater than idle' })
}
it('uses a string detail as the 422 message', check422Text)

/** @returns {Promise<void>} Checks FastAPI validation details are joined into one message. */
async function check422List() {
  const detail = [{ loc: ['body', 'report_interval_seconds'], msg: 'Value error, too small' },
    { loc: ['body', 'price_gbp_per_kwh'], msg: 'Input should be finite' }]
  fetch.mockResolvedValue(answer(422, { detail }))
  await expect(saveSettings(body)).rejects.toMatchObject({
    status: 422, message: 'report_interval_seconds: Value error, too small; price_gbp_per_kwh: Input should be finite',
  })
}
it('joins a list detail of a 422 into one message', check422List)

/** @returns {Promise<void>} Checks 409 keeps its status. */
async function check409() {
  fetch.mockResolvedValue(answer(409, { detail: 'Reload' }))
  await expect(saveSettings(body)).rejects.toMatchObject({ status: 409 })
}
it('exposes 409 with its status', check409)

/** @returns {Promise<void>} Checks a failed network request stays visible. */
async function checkNetwork() {
  fetch.mockRejectedValue(new TypeError('network down'))
  await expect(saveSettings(body)).rejects.toThrow('network down')
}
it('exposes network failures of a save', checkNetwork)

/** @returns {Promise<void>} Checks sample mode: no fetch, save stays in memory with a new version. */
async function checkSample() {
  vi.stubEnv('VITE_SETTINGS_DATA_SOURCE', '')
  expect(await getSettings()).toEqual(sample)
  const saved = await saveSettings({ ...sample, report_interval_seconds: 900 })
  expect(saved).toMatchObject({ report_interval_seconds: 900, settings_version: sample.settings_version + 1 })
  expect(await getSettings()).toEqual(saved)
  expect(fetch).not.toHaveBeenCalled()
}
it('keeps a sample save in memory only', checkSample)
