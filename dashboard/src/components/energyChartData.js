import { ujToKw, ujToKwh } from '../data/units.js'

const ukTime = new Intl.DateTimeFormat('en-GB', {
  timeZone: 'Europe/London', hour: '2-digit', minute: '2-digit', hourCycle: 'h23',
})

export function formatUkTime(timestamp) {
  return ukTime.format(new Date(timestamp))
}

// Keep the UTC timestamp as the unique chart key. Two different buckets can
// share a UK clock label when the clocks go back.
export function toEnergyChartData(points) {
  return points.map(({ timestamp, energy_uj }) => ({
    timestamp,
    power_kw: ujToKw(energy_uj, 1800),
    energy_kwh: ujToKwh(energy_uj),
  }))
}
