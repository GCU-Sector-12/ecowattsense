// Tests for API unit conversions, currency and relative-time boundaries.
import { describe, expect, it } from 'vitest'
import { formatGbp, timeAgo, ujToKwh, ujToKw } from './units.js'

describe('API energy units', () => {
  it('converts microjoules to kWh', () => {
    expect(ujToKwh(3_600_000_000_000)).toBe(1)
    expect(ujToKwh(0)).toBe(0)
  })
  it('converts microjoules over a time period to average kW', () => {
    expect(ujToKw(1_800_000_000_000, 1800)).toBe(1)
    expect(() => ujToKw(1, 0)).toThrow(RangeError)
  })
  it('formats pounds sterling', () => {
    expect(formatGbp(1.2)).toBe('£1.20')
  })
  it('formats relative times with a fixed reference date', () => {
    const now = '2025-03-10T14:32:00Z'
    expect(timeAgo('2025-03-10T14:30:00Z', now)).toBe('2 mins ago')
    expect(timeAgo('2025-03-10T12:32:00Z', now)).toBe('2 hours ago')
    expect(timeAgo(null, now)).toBe('Never')
  })
})

describe('unit helper boundaries', () => {
  it.each([-1, NaN, Infinity, undefined])('rejects invalid duration %s', (seconds) => {
    expect(() => ujToKw(1, seconds)).toThrow(RangeError)
  })
  it('converts a non-half-hour interval', () => {
    expect(ujToKw(9_000_000_000, 300)).toBeCloseTo(0.03)
  })
  it('formats zero, negative values and rounding in GBP', () => {
    expect(formatGbp(0)).toBe('£0.00')
    expect(formatGbp(-1.25)).toBe('-£1.25')
    expect(formatGbp(1.236)).toBe('£1.24')
  })
  it.each([
    [0, 'just now'], [59, 'just now'], [60, '1 min ago'],
    [3599, '59 mins ago'], [3600, '1 hour ago'], [86400, '1 day ago'],
    [172800, '2 days ago'], [-60, 'just now'],
  ])('formats %s elapsed seconds as %s', (seconds, expected) => {
    const now = new Date('2025-03-10T14:32:00Z')
    expect(timeAgo(new Date(now.getTime() - seconds * 1000), now)).toBe(expected)
  })
  it('handles invalid dates without displaying NaN', () => {
    expect(timeAgo('invalid', '2025-03-10T14:32:00Z')).toBe('Unknown')
    expect(timeAgo('2025-03-10T14:32:00Z', 'invalid')).toBe('Unknown')
  })
})
