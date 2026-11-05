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
