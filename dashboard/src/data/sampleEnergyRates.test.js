// Sample energy-rate store checks: validation, memory-only save, independent copies and reset.
import { afterEach, expect, it, vi } from 'vitest'
import { readSampleRates, resetSampleRates, saveSampleRates, validateRates } from './sampleEnergyRates.js'

/** @returns {void} Removes saved rates and request spies after each case. */
function restore() {
  resetSampleRates()
  vi.restoreAllMocks()
}
afterEach(restore)

it('keeps both seeded device types and independent snapshots', () => {
  const rates = readSampleRates()
  expect(rates.map(rate => rate.device_type)).toEqual(['desktop', 'laptop'])
  rates[0].active_watts = 999
  expect(readSampleRates()[0].active_watts).toBe(55)
})

it('saves all watts in memory without HTTP requests or browser storage', () => {
  const request = vi.spyOn(globalThis, 'fetch')
  const storage = vi.spyOn(Storage.prototype, 'setItem')
  const rates = readSampleRates().map(rate => ({ ...rate, active_watts: 10, idle_watts: 5, unused_watts: 0 }))
  const saved = saveSampleRates(rates)
  expect(readSampleRates()).toEqual(rates)
  saved[0].active_watts = 999
  expect(readSampleRates()[0].active_watts).toBe(10)
  expect(request).not.toHaveBeenCalled()
  expect(storage).not.toHaveBeenCalled()
  resetSampleRates()
  expect(readSampleRates()[0].active_watts).toBe(55)
})

/** @param {*} value - Invalid watts. @returns {void} Confirms failed saves leave every saved row untouched. */
function checkInvalid(value) {
  const before = readSampleRates()
  const rates = structuredClone(before)
  rates[0].active_watts = value
  expect(validateRates(rates)['0.active_watts']).toBeDefined()
  expect(() => saveSampleRates(rates)).toThrow()
  expect(readSampleRates()).toEqual(before)
}
it.each([-1, '', NaN, Infinity])('rejects invalid watts %s atomically', checkInvalid)
