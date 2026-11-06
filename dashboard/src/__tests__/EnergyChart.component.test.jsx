import { beforeEach, expect, it, vi } from 'vitest'
import { screen } from '@testing-library/react'
import { AreaChart } from '@mantine/charts'
import EnergyChart from '../components/EnergyChart.jsx'
import { renderWithMantine } from '../test/render.jsx'

// The adapter and empty state are this PR's scope. The actual chart also
// renders through the existing dashboard navigation integration test.
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

it('passes converted data and UK time formatters to the chart', () => {
  renderWithMantine(<EnergyChart points={[
    { timestamp: '2025-07-10T00:00:00Z', energy_uj: 1_800_000_000_000 },
  ]} />)
  const props = AreaChart.mock.lastCall[0]
  expect(props.dataKey).toBe('timestamp')
  expect(props.data).toEqual([
    { timestamp: '2025-07-10T00:00:00Z', power_kw: 1, energy_kwh: 0.5 },
  ])
  expect(props.xAxisProps.tickFormatter(props.data[0].timestamp)).toBe('01:00')
  expect(props.tooltipProps.labelFormatter(props.data[0].timestamp)).toBe('01:00')
})
