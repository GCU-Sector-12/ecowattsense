// Provider simulation tests. Run with npm test to check development and production behavior.
import { afterEach, expect, it, vi } from 'vitest'
import { getDevices, getSummary, getSettings, getDeviceHistory } from './index.js'
import history from './history.json'

/** @returns {Promise<object>} Requests the known sample history. */
function loadHistory() {
  return getDeviceHistory(history.device_id)
}

/** @returns {void} Restores the URL, environment and timers after each case. */
function restoreEnvironment() {
  window.history.replaceState(null, '', '/')
  vi.useRealTimers()
  vi.unstubAllEnvs()
}
afterEach(restoreEnvironment)

/**
 * Checks that each provider honours failure and can recover on a later request.
 * @param {Function} load - Sample provider.
 * @returns {Promise<void>} Resolves after the assertions.
 */
async function checkFailure(load) {
  vi.stubEnv('DEV', true)
  window.history.replaceState(null, '', '/?fail=1')
  await expect(load()).rejects.toThrow('Simulated data failure')
  window.history.replaceState(null, '', '/')
  await expect(load()).resolves.toBeTruthy()
}
it.each([getDevices, getSummary, getSettings, loadHistory])('fails and recovers: %s', checkFailure)

/** @returns {Promise<void>} Checks that delay defers a successful request. */
async function checkDelay() {
  vi.stubEnv('DEV', true)
  vi.useFakeTimers()
  window.history.replaceState(null, '', '/?delay=2000')
  const resolved = vi.fn()
  const request = getDevices().then(resolved)
  await vi.advanceTimersByTimeAsync(1999)
  expect(resolved).not.toHaveBeenCalled()
  await vi.advanceTimersByTimeAsync(1)
  await request
  expect(resolved).toHaveBeenCalledOnce()
}
it('waits for the requested delay', checkDelay)

/** @returns {Promise<void>} Checks that failure occurs after the requested delay. */
async function checkDelayedFailure() {
  vi.stubEnv('DEV', true)
  vi.useFakeTimers()
  window.history.replaceState(null, '', '/?delay=2000&fail=1')
  const failed = vi.fn()
  const request = getDevices().catch(failed)
  await vi.advanceTimersByTimeAsync(1999)
  expect(failed).not.toHaveBeenCalled()
  await vi.advanceTimersByTimeAsync(1)
  await request
  expect(failed).toHaveBeenCalledOnce()
}
it('waits before showing a simulated failure', checkDelayedFailure)

/** @returns {Promise<void>} Checks that production ignores both URL switches. */
async function checkProduction() {
  vi.stubEnv('DEV', false)
  vi.useFakeTimers()
  window.history.replaceState(null, '', '/?delay=2000&fail=1')
  await expect(getDevices()).resolves.toHaveProperty('devices')
  expect(vi.getTimerCount()).toBe(0)
}
it('ignores controls in production', checkProduction)

/** @param {string} delay - Invalid timer value. @returns {Promise<void>} Checks no timer starts. */
async function checkInvalidDelay(delay) {
  vi.stubEnv('DEV', true)
  vi.useFakeTimers()
  window.history.replaceState(null, '', '/?delay=' + delay)
  await expect(getDevices()).resolves.toHaveProperty('devices')
  expect(vi.getTimerCount()).toBe(0)
}
it.each(['-1', 'invalid', 'Infinity', '0', '1.5', '2147483648'])('ignores invalid delay %s', checkInvalidDelay)
