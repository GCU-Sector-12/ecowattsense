// Unit checks for history durations, numeric states and calendar dates used by HistoryPage.
import { describe, expect, it } from 'vitest'
import {
  calendarDay, dayRange, energyByHalfHour, formatDuration, historyDate, historyState, reportTime,
} from './historyView.js'

describe('history display values', () => {
  it.each([
    [0, '0 h 0 min'], [59, '0 h 0 min'], [60, '0 h 1 min'],
    [3660, '1 h 1 min'], [36000, '10 h 0 min'], [90000, '25 h 0 min'],
  ])('formats %i elapsed seconds as %s', (seconds, expected) => {
    expect(formatDuration(seconds)).toBe(expected)
  })

  it('maps the contract states without reversing active and unused', () => {
    expect([0, 1, 2, 99].map(historyState)).toEqual(['unused', 'idle', 'active', 'no_data'])
  })

  it('formats a local calendar day for the date input', () => {
    expect(calendarDay(new Date(2026, 0, 5))).toBe('2026-01-05')
  })

  it('keeps report times and sample dates in UTC', () => {
    expect(reportTime('2025-03-10T08:30:00Z')).toBe('08:30')
    expect(reportTime('2025-03-10T09:30:00+01:00')).toBe('08:30')
    expect(historyDate('2025-03-10T00:00:00Z')).toBe('10 March 2025')
  })
})

describe('real history helpers', () => {
  it('turns a calendar day into a UTC day range with an exclusive end', () => {
    expect(dayRange('2026-11-14')).toEqual({ from: '2026-11-14T00:00:00Z', to: '2026-11-15T00:00:00Z' })
    expect(dayRange('2026-12-31')).toEqual({ from: '2026-12-31T00:00:00Z', to: '2027-01-01T00:00:00Z' })
  })

  it('sums energy into half-hour UTC buckets in time order', () => {
    const reports = [
      { timestamp: '2026-11-14T00:25:00Z', energy_uj: 5 },
      { timestamp: '2026-11-14T00:05:00Z', energy_uj: 3 },
      { timestamp: '2026-11-14T00:30:00Z', energy_uj: 7 },
      { timestamp: '2026-11-14T01:10:00+00:00', energy_uj: 0 },
    ]
    expect(energyByHalfHour(reports)).toEqual([
      { timestamp: '2026-11-14T00:00:00Z', energy_uj: 8 },
      { timestamp: '2026-11-14T00:30:00Z', energy_uj: 7 },
      { timestamp: '2026-11-14T01:00:00Z', energy_uj: 0 },
    ])
    expect(energyByHalfHour([])).toEqual([])
  })

  it('puts a full day of 288 reports into 48 buckets', () => {
    const start = Date.parse('2026-11-14T00:00:00Z')
    const reports = Array.from({ length: 288 }, (_, i) => ({
      timestamp: new Date(start + i * 300000).toISOString(), energy_uj: 1,
    }))
    const buckets = energyByHalfHour(reports)
    expect(buckets).toHaveLength(48)
    expect(buckets.every(bucket => bucket.energy_uj === 6)).toBe(true)
  })
})
