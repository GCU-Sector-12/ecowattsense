import { describe, expect, it } from 'vitest'
import { formatUkTime, toEnergyChartData } from './energyChartData.js'

describe('energy chart data', () => {
  it('converts 30-minute bucket energy to average kW and bucket kWh', () => {
    const points = [{ timestamp: '2025-03-10T00:00:00Z', energy_uj: 1_800_000_000_000 }]
    expect(toEnergyChartData(points)).toEqual([
      { timestamp: points[0].timestamp, power_kw: 1, energy_kwh: 0.5 },
    ])
    expect(points[0]).not.toHaveProperty('power_kw')
  })

  it('preserves zero-energy buckets and an empty series', () => {
    expect(toEnergyChartData([{ timestamp: '2025-03-10T00:00:00Z', energy_uj: 0 }])[0])
      .toMatchObject({ power_kw: 0, energy_kwh: 0 })
    expect(toEnergyChartData([])).toEqual([])
  })

  it.each([
    ['2025-01-10T00:00:00Z', '00:00'],
    ['2025-07-10T00:00:00Z', '01:00'],
    ['2025-07-09T23:00:00Z', '00:00'],
    ['2025-03-30T00:30:00Z', '00:30'],
    ['2025-03-30T01:00:00Z', '02:00'],
  ])('formats %s in UK local time as %s', (timestamp, expected) => {
    expect(formatUkTime(timestamp)).toBe(expected)
  })

  it('keeps repeated autumn clock labels as separate UTC buckets', () => {
    const times = ['2025-10-26T00:30:00Z', '2025-10-26T01:30:00Z']
    const data = toEnergyChartData(times.map((timestamp) => ({ timestamp, energy_uj: 0 })))
    expect(data.map(({ timestamp }) => timestamp)).toEqual(times)
    expect(times.map(formatUkTime)).toEqual(['01:30', '01:30'])
  })
})
