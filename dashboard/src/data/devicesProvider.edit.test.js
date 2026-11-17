// Sample-mode computer edits stay in memory and never call the network.
import { afterEach, expect, it, vi } from 'vitest'
import sample from '../data/devices.json'

afterEach(() => {
  vi.unstubAllEnvs()
  vi.unstubAllGlobals()
  vi.resetModules()
})

/** @returns {Promise<void>} Verifies sample edits survive provider reads in the same session. */
async function keepsSampleEditInMemory() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', '')
  vi.stubGlobal('fetch', vi.fn())
  const { loadDevices, updateDevice } = await import('../data/devicesProvider.js')
  const device = sample.devices[0]
  const updated = await updateDevice(device.device_id, { display_name: 'Session rename', archived: true })
  expect(updated.display_name).toBe('Session rename')
  expect(updated.archived).toBe(true)
  expect((await loadDevices()).devices[0]).toMatchObject({
    device_id: device.device_id, display_name: 'Session rename', archived: true,
  })
  expect(fetch).not.toHaveBeenCalled()
}
it('keeps sample computer edits in memory without a network request', keepsSampleEditInMemory)

/** @returns {Promise<void>} Verifies an unknown sample device has the documented 404 status. */
async function rejectsMissingSampleDevice() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', '')
  const { updateDevice } = await import('../data/devicesProvider.js')
  await expect(updateDevice('missing-device', { archived: true })).rejects.toMatchObject({ status: 404 })
}
it('returns 404 when a sample computer does not exist', rejectsMissingSampleDevice)

/** @param {string} call - Provider call that must end the session on 401. @returns {Promise<void>} Checks the session is cleared. */
async function endsSessionOn401(call) {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: false, status: 401, json: async () => ({}) }))
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'daro' }))
  const provider = await import('../data/devicesProvider.js')
  await expect(provider[call]('device-1', {})).rejects.toMatchObject({ status: 401 })
  expect(sessionStorage.getItem('ews-sample-session')).toBeNull()
}
it.each(['loadLocations', 'updateDevice'])('ends the session when %s gets 401', endsSessionOn401)
