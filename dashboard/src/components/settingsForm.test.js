// Tests for the pure Settings form rules: form values, validation, changes and the PUT body.
import { expect, it } from 'vitest'
import { buildPayload, isChanged, toFormValues, validateForm } from './settingsForm.js'

const base = {
  settings_version: 7, report_interval_seconds: 300, ping_interval_seconds: 60,
  idle_threshold_seconds: 600, unused_threshold_seconds: 3600, no_data_after_cycles: 3,
  tariff_valid_from: '2026-09-14T00:00:00Z', price_gbp_per_kwh: 0.25, co2_kg_per_kwh: 0.207,
}
const NOW = new Date('2026-11-15T10:42:37.123Z')

it('shows thresholds in minutes', () => {
  expect(toFormValues(base)).toEqual({
    report_interval_seconds: 300, idle_minutes: 10, unused_minutes: 60,
    price_gbp_per_kwh: 0.25, co2_kg_per_kwh: 0.207,
  })
})

it('accepts the values of a normal GET answer', () => {
  expect(validateForm(toFormValues(base))).toEqual({})
})

/** @param {number} value - Report interval. @returns {void} Checks the 60 to 3600 range. */
function checkInterval(value) {
  const errors = validateForm({ ...toFormValues(base), report_interval_seconds: value })
  expect(errors.report_interval_seconds).toBe('Enter a number from 60 to 3600.')
}
it.each([59, 3601])('rejects a report interval of %s', checkInterval)

/** @param {number} value - Report interval. @returns {void} Checks the limits are allowed. */
function checkIntervalEdge(value) {
  expect(validateForm({ ...toFormValues(base), report_interval_seconds: value })).toEqual({})
}
it.each([60, 3600])('accepts a report interval of %s', checkIntervalEdge)

it('requires unused to be greater than idle', () => {
  const errors = validateForm({ ...toFormValues(base), idle_minutes: 30, unused_minutes: 30 })
  expect(errors.unused_minutes).toBe('Unused after must be greater than Idle after.')
})

it('rejects a negative price and a negative CO2 factor', () => {
  const errors = validateForm({ ...toFormValues(base), price_gbp_per_kwh: -0.01, co2_kg_per_kwh: -1 })
  expect(errors.price_gbp_per_kwh).toBe('Enter 0 or more.')
  expect(errors.co2_kg_per_kwh).toBe('Enter 0 or more.')
})

it('rejects an empty field', () => {
  const errors = validateForm({ ...toFormValues(base), report_interval_seconds: '', idle_minutes: '' })
  expect(errors.report_interval_seconds).toBeDefined()
  expect(errors.idle_minutes).toBeDefined()
})

it('detects a change against the last GET answer', () => {
  const values = toFormValues(base)
  expect(isChanged(values, base)).toBe(false)
  expect(isChanged({ ...values, idle_minutes: 11 }, base)).toBe(true)
})

it('sends all nine fields and keeps the tariff time when price and CO2 are unchanged', () => {
  const values = { ...toFormValues(base), report_interval_seconds: 600, idle_minutes: 15 }
  expect(buildPayload(values, base, NOW)).toEqual({
    ...base, report_interval_seconds: 600, idle_threshold_seconds: 900,
  })
})

it('starts a new tariff period now, rounded down to the minute, when the price changes', () => {
  const payload = buildPayload({ ...toFormValues(base), price_gbp_per_kwh: 0.3 }, base, NOW)
  expect(payload.tariff_valid_from).toBe('2026-11-15T10:42:00Z')
  expect(payload.price_gbp_per_kwh).toBe(0.3)
})

it('starts a new tariff period when only CO2 changes', () => {
  const payload = buildPayload({ ...toFormValues(base), co2_kg_per_kwh: 0.2 }, base, NOW)
  expect(payload.tariff_valid_from).toBe('2026-11-15T10:42:00Z')
})

it('keeps a threshold that is not whole minutes unchanged', () => {
  const odd = { ...base, idle_threshold_seconds: 90 }
  expect(buildPayload(toFormValues(odd), odd, NOW).idle_threshold_seconds).toBe(90)
})
