// Regression tests for adaptive energy and power displays in dashboard views.
import { beforeEach, expect, it, vi } from 'vitest'
import { screen, within } from '@testing-library/react'
import { AreaChart, BarChart } from '@mantine/charts'
import { chooseDisplayUnit } from '../data/units.js'
import { getSummary } from '../data/index.js'
import EnergyChart from '../components/EnergyChart.jsx'
import EnergyCard from '../components/EnergyCard.jsx'
import HistoryReports from '../components/HistoryReports.jsx'
import EnergyPage from '../pages/EnergyPage.jsx'
import { renderWithMantine } from '../test/render.jsx'

vi.mock('@mantine/charts', () => ({
  AreaChart: vi.fn(() => <div aria-label="Power chart" />),
  BarChart: vi.fn(() => <div aria-label="Daily energy chart" />),
}))
vi.mock('../data/index.js', () => ({ getSummary: vi.fn() }))
beforeEach(() => vi.clearAllMocks())

/**
 * Checks exact unit boundaries without rounding the input first.
 * @param {number} value - Input in kWh or kW.
 * @param {string} kind - Quantity name.
 * @param {string} unit - Expected label.
 * @param {number} multiplier - Expected scale.
 * @returns {void} Throws on an assertion failure.
 */
function checkUnit(value, kind, unit, multiplier) {
  expect(chooseDisplayUnit(value, kind)).toEqual({ unit, multiplier })
}
it.each([
  [0, 'energy', 'Wh', 1000], [0.004, 'energy', 'Wh', 1000],
  [0.9999, 'energy', 'Wh', 1000], [1, 'energy', 'kWh', 1], [2, 'energy', 'kWh', 1],
  [0, 'power', 'W', 1000], [0.01, 'power', 'W', 1000],
  [0.9999, 'power', 'W', 1000], [1, 'power', 'kW', 1], [2, 'power', 'kW', 1],
])('selects units for %s %s', checkUnit)

/** Checks a laptop chart, including the actual tooltip content and axis. @returns {void|Promise<void>} Throws on an assertion failure. */
function checkLaptop() {
  renderWithMantine(<EnergyChart points={[{ timestamp: '2026-11-20T00:00:00Z', energy_uj: 18_000_000_000 }]} />)
  const props = AreaChart.mock.lastCall[0]
  expect(props.yAxisLabel).toBe('Power (W)')
  expect(props.data[0][props.series[0].name]).toBe(10)
  expect(props.yAxisProps.domain[1]).toBeLessThanOrEqual(20)
  const tooltip = props.tooltipProps.content({ active: true, label: props.data[0].timestamp, payload: [{ payload: props.data[0] }] })
  renderWithMantine(tooltip)
  expect(screen.getByText('Power: 10.0 W')).toBeInTheDocument()
  expect(screen.getByText('Energy: 5.000 Wh')).toBeInTheDocument()
}
it('shows laptop power in W without a flat-looking axis', checkLaptop)

/** Checks one common power scale when the series crosses 1 kW. @returns {void|Promise<void>} Throws on an assertion failure. */
function checkMixedPower() {
  renderWithMantine(<EnergyChart points={[
    { timestamp: '2026-11-20T00:00:00Z', energy_uj: 18_000_000_000 },
    { timestamp: '2026-11-20T00:30:00Z', energy_uj: 1_800_000_000_000 },
  ]} />)
  const props = AreaChart.mock.lastCall[0]
  expect(props.yAxisLabel).toBe('Power (kW)')
  expect(props.data.map(point => point[props.series[0].name])).toEqual([0.01, 1])
}
it('uses the peak to select one power unit for the entire chart', checkMixedPower)

/** Checks that a small energy total remains visible and tariffs stay per kWh. @returns {void|Promise<void>} Throws on an assertion failure. */
function checkSmallCard() {
  renderWithMantine(<EnergyCard energy_uj={14_400_000_000} cost_gbp={0.001} co2_kg={0.001}
    price_gbp_per_kwh={0.25} co2_kg_per_kwh={0.207} />)
  expect(screen.getByText('4.0 Wh')).toBeInTheDocument()
}
it('shows a 0.004 kWh total as 4 Wh on the dashboard card', checkSmallCard)

/** @param {string} date - Requested date. @returns {Promise<object>} Small daily API total. */
async function smallSummary(date) {
  return { date, energy_uj: 14_400_000_000, cost_gbp: 0.001, co2_kg: 0.001 }
}

/** Checks that daily bars keep their magnitude and match headings and table units. @returns {void|Promise<void>} Throws on an assertion failure. */
async function checkSmallDays() {
  getSummary.mockImplementation(smallSummary)
  renderWithMantine(<EnergyPage />)
  expect(await screen.findByText('28.00 Wh')).toBeInTheDocument()
  expect(screen.getByText('Energy per day (Wh)')).toBeInTheDocument()
  expect(screen.getByRole('columnheader', { name: 'Energy' })).toBeInTheDocument()
  expect(screen.getAllByRole('cell', { name: '4.00 Wh' })).toHaveLength(7)
  const props = BarChart.mock.lastCall[0]
  expect(props.data[0].energy).toBe(4)
  expect(props.yAxisLabel).toBe('Energy (Wh)')
  expect(props.valueFormatter(4)).toBe('4.00 Wh')
}
it('shows small daily energy in Wh in bars, totals and the table', checkSmallDays)

/** Checks report rows choose their own units and preserve the boundary at 1 kWh. @returns {void|Promise<void>} Throws on an assertion failure. */
function checkReports() {
  const base = { state: 2, cpu_percent: 5, idle_seconds: 0, on_battery: false }
  renderWithMantine(<HistoryReports reports={[
    { ...base, timestamp: '2026-11-20T00:00:00Z', energy_uj: 14_400_000_000 },
    { ...base, timestamp: '2026-11-20T00:05:00Z', energy_uj: 3_600_000_000_000 },
  ]} />)
  const table = within(screen.getByRole('table', { name: 'History reports' }))
  expect(table.getByText('4.000 Wh')).toBeInTheDocument()
  expect(table.getByText('1.000 kWh')).toBeInTheDocument()
}
it('uses adaptive units in history report rows', checkReports)

/** @param {string} date - Requested date. @returns {Promise<object>} Mixed-size API total. */
async function mixedSummary(date) {
  const energy_uj = date.endsWith('10') ? 3_600_000_000_000 : 14_400_000_000
  return { date, energy_uj, cost_gbp: 0, co2_kg: 0 }
}

/** Verifies a large day selects kWh for all bars without destroying smaller readings. @returns {void|Promise<void>} Throws on an assertion failure. */
async function checkMixedDays() {
  getSummary.mockImplementation(mixedSummary)
  renderWithMantine(<EnergyPage />)
  expect(await screen.findByText('1.02 kWh')).toBeInTheDocument()
  const props = BarChart.mock.lastCall[0]
  expect(props.yAxisLabel).toBe('Energy (kWh)')
  expect(props.data[0].energy).toBe(0.004)
  expect(props.data[6].energy).toBe(1)
  expect(props.valueFormatter(0.004)).toBe('0.004 kWh')
  expect(screen.getAllByRole('cell', { name: '4.00 Wh' })).toHaveLength(6)
  expect(screen.getByRole('cell', { name: '1.00 kWh' })).toBeInTheDocument()
}
it('preserves small bars and independently labelled rows in a mixed range', checkMixedDays)

/** @param {number} value - Invalid input. @returns {void} Throws on an assertion failure. */
function checkInvalid(value) {
  expect(() => chooseDisplayUnit(value)).toThrow(RangeError)
}
it.each([-1, NaN, Infinity])('rejects invalid energy %s', checkInvalid)
