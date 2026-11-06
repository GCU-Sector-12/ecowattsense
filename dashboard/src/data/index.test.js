import { describe, expect, it } from 'vitest'
import { getSummary, getDevices, getDeviceHistory, getSettings } from './index.js'
import history from './history.json'
import sample from '../../../docs/design/dashboard-sample-data.json'

describe('sample data contract', () => {
  it('keeps summary API values and excludes presentation fields', async () => {
    const { display, ...expected } = sample.summary
    expect(await getSummary()).toEqual(expected)
  })

  it('keeps device API values and excludes presentation fields', async () => {
    const expected = sample.devices.map(({ last_seen_text, estimated_watts, ...device }) => device)
    expect(await getDevices()).toEqual({ devices: expected })
  })

  it('does not relabel sample readings as another day', async () => {
    expect(await getSummary('2025-03-11')).toMatchObject({
      date: '2025-03-11', energy_uj: 0, cost_gbp: 0, co2_kg: 0, series: [],
    })
  })

  it('returns independent copies from every provider', async () => {
    for (const getData of [getSummary, getDevices, getSettings,
      () => getDeviceHistory(history.device_id)]) {
      const first = getData()
      expect(first).toBeInstanceOf(Promise)
      const value = await first
      const expected = structuredClone(value)
      if (value.series) value.series[0].energy_uj = -1
      if (value.devices) value.devices[0].display_name = 'Changed'
      if (value.reports) value.reports[0].energy_uj = -1
      if (value.settings_version) value.settings_version = -1
      expect(await getData()).toEqual(expected)
    }
  })

  it('has 48 ordered history reports with the response fields and matching totals', async () => {
    const result = await getDeviceHistory(history.device_id)
    expect(result.reports).toHaveLength(48)
    expect(result.totals).toEqual(history.totals)
    const fields = ['timestamp', 'interval_seconds', 'state', 'idle_seconds',
      'cpu_percent', 'screen_locked', 'on_battery', 'energy_uj', 'energy_source'].sort()
    for (const [index, report] of result.reports.entries()) {
      expect(Object.keys(report).sort()).toEqual(fields)
      expect(report.interval_seconds).toBe(1800)
      expect([0, 1, 2]).toContain(report.state)
      expect(Number.isSafeInteger(report.energy_uj)).toBe(true)
      expect(Date.parse(report.timestamp)).toBeGreaterThanOrEqual(Date.parse(result.from))
      expect(Date.parse(report.timestamp)).toBeLessThan(Date.parse(result.to))
      if (index) expect(Date.parse(report.timestamp) -
        Date.parse(result.reports[index - 1].timestamp)).toBe(1800000)
    }
    expect(result.totals.active_time_seconds + result.totals.idle_time_seconds +
      result.totals.unused_time_seconds).toBe(86400)
  })

  it('includes from, excludes to, and recomputes totals for the selected range', async () => {
    const from = '2025-03-10T08:00:00.000Z'
    const to = '2025-03-10T09:00:00.000Z'
    const result = await getDeviceHistory(history.device_id, from, to)
    expect(result.from).toBe(from)
    expect(result.to).toBe(to)
    expect(result.reports.map(({ timestamp }) => timestamp)).toEqual([
      '2025-03-10T08:00:00Z', '2025-03-10T08:30:00Z',
    ])
    expect(result.totals).toEqual({
      active_time_seconds: 3600, idle_time_seconds: 0, unused_time_seconds: 0,
      energy_uj: 2214000000000, cost_gbp: 0.1537, co2_kg: 0.1273,
    })
  })

  it('returns zero totals for a range with no reports', async () => {
    const result = await getDeviceHistory(history.device_id,
      '2025-03-11T00:00:00Z', '2025-03-12T00:00:00Z')
    expect(result.reports).toEqual([])
    expect(Object.values(result.totals)).toEqual([0, 0, 0, 0, 0, 0])
  })

  it('handles a single bound without changing the other sample bound', async () => {
    const result = await getDeviceHistory(history.device_id, undefined, '2025-03-10T01:00:00Z')
    expect(result.from).toBe(history.from)
    expect(result.reports).toHaveLength(2)
  })

  it('does not return another computer history for an unknown device', async () => {
    expect(await getDeviceHistory('missing-device')).toBeNull()
  })

  it.each([
    ['invalid', history.to],
    [history.to, history.from],
    [history.from, history.from],
    [history.from, '2025-04-11T00:00:00Z'],
  ])('rejects an invalid history range: %s to %s', async (from, to) => {
    await expect(getDeviceHistory(history.device_id, from, to)).rejects.toThrow(RangeError)
  })
})
