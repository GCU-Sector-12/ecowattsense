import { beforeEach, expect, it, vi } from 'vitest'
import { screen } from '@testing-library/react'
import { AreaChart } from '@mantine/charts'
import EnergyChart from '../components/EnergyChart.jsx'
import { renderWithMantine } from '../test/render.jsx'

vi.mock('@mantine/charts', () => ({
  AreaChart: vi.fn(() => <div aria-label="Energy chart" />),
  DonutChart: vi.fn(() => null),
}))

beforeEach(() => vi.clearAllMocks())

it('shows the empty state instead of mounting a chart with no buckets', () => {
  renderWithMantine(<EnergyChart points={[]} />)
  expect(screen.getByText('No data for this day')).toBeInTheDocument()
  expect(AreaChart).not.toHaveBeenCalled()
})

it('passes converted data, UK time ticks and a detailed tooltip to the chart', () => {
  renderWithMantine(<EnergyChart points={[
    { timestamp: '2025-07-10T00:00:00Z', energy_uj: 1_800_000_000_000 },
  ]} />)
  const props = AreaChart.mock.lastCall[0]
  expect(props.dataKey).toBe('timestamp')
  expect(props.data).toEqual([
    { timestamp: '2025-07-10T00:00:00Z', power_kw: 1, energy_kwh: 0.5 },
  ])
  expect(props.xAxisProps.ticks).toEqual(['2025-07-10T00:00:00Z'])
  expect(props.xAxisProps.tickFormatter(props.data[0].timestamp)).toBe('01:00')
  expect(props.tooltipProps.labelFormatter(props.data[0].timestamp)).toBe('01:00')

  const tooltip = props.tooltipProps.content({
    active: true,
    label: props.data[0].timestamp,
    payload: [{ payload: props.data[0] }],
  })
  expect(tooltip.props.children.props.children.map((child) => child.props.children))
    .toEqual(['01:00', ['Power: ', '1.0', ' kW'], ['Energy: ', '0.500', ' kWh']])
})

it('scales the Y axis to a rounded maximum for higher power readings', () => {
  renderWithMantine(<EnergyChart points={[
    { timestamp: '2025-03-10T00:00:00Z', energy_uj: 3 * 1_800_000_000_000 },
  ]} />)
  const axis = AreaChart.mock.lastCall[0].yAxisProps
  expect(axis.domain).toEqual([0, 3])
  expect(axis.ticks).toEqual([0, 0.75, 1.5, 2.25, 3])
})

it('keeps a useful Y axis for an all-zero series', () => {
  renderWithMantine(<EnergyChart points={[
    { timestamp: '2025-03-10T00:00:00Z', energy_uj: 0 },
  ]} />)
  expect(AreaChart.mock.lastCall[0].yAxisProps.domain).toEqual([0, 0.5])
})

it('places ticks on UK four-hour boundaries across the spring clock change', () => {
  const points = Array.from({ length: 46 }, (_, index) => ({
    timestamp: new Date(Date.parse('2025-03-30T00:00:00Z') + index * 30 * 60 * 1000).toISOString(),
    energy_uj: 0,
  }))
  renderWithMantine(<EnergyChart points={points} />)
  expect(AreaChart.mock.lastCall[0].xAxisProps.ticks).toEqual([
    '2025-03-30T00:00:00.000Z',
    '2025-03-30T03:00:00.000Z',
    '2025-03-30T07:00:00.000Z',
    '2025-03-30T11:00:00.000Z',
    '2025-03-30T15:00:00.000Z',
    '2025-03-30T19:00:00.000Z',
  ])
})

it('keeps a useful tick when a partial day has no four-hour boundary', () => {
  renderWithMantine(<EnergyChart points={[
    { timestamp: '2025-03-10T13:30:00Z', energy_uj: 0 },
    { timestamp: '2025-03-10T14:00:00Z', energy_uj: 0 },
  ]} />)
  expect(AreaChart.mock.lastCall[0].xAxisProps.ticks).toEqual(['2025-03-10T13:30:00Z'])
})
