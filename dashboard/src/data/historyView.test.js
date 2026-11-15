// Unit checks for history durations, numeric states and calendar dates used by HistoryPage.
import { describe, expect, it } from 'vitest'
import { calendarDay, formatDuration, historyDate, historyState, reportTime } from './historyView.js'

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
