// Converts API buckets and selects consistent display scales for EnergyChart.
import { ujToKw, ujToKwh, chooseDisplayUnit } from '../data/units.js'

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

/**
 * Adds display values without changing the canonical kW and kWh fields.
 * @param {object[]} data - Converted half-hour buckets from toEnergyChartData.
 * @returns {{data: object[], powerUnit: string}} Points sharing one scale per quantity.
 * @throws {RangeError} If a peak is not a valid energy or power reading.
 */
export function scaleEnergyChartData(data) {
  const power = chooseDisplayUnit(Math.max(0, ...data.map(point => point.power_kw)), 'power')
  const energy = chooseDisplayUnit(Math.max(0, ...data.map(point => point.energy_kwh)))
  return {
    powerUnit: power.unit,
    data: data.map(point => ({
      ...point,
      power: point.power_kw * power.multiplier,
      energy: point.energy_kwh * energy.multiplier,
      powerUnit: power.unit,
      energyUnit: energy.unit,
    })),
  }
}
