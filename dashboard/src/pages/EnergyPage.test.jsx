// Energy Usage page integration tests. Run with npm test.
import { expect, it, vi } from 'vitest'
import { screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { BarChart } from '@mantine/charts'
import EnergyPage from '../pages/EnergyPage.jsx'
import { renderWithMantine } from '../test/render.jsx'

vi.mock('@mantine/charts', () => ({
  BarChart: vi.fn(() => <div data-testid="energy-bar-chart" />),
}))

vi.mock('../data/index.js', () => ({
  getSummary: vi.fn(async (date) => ({
    date,
    energy_uj: 3600000000000,
    cost_gbp: 0.25,
    co2_kg: 0.207,
    counts: { active: 1, idle: 0, unused: 0, no_data: 0, total: 1 },
    series: [],
  })),
}))

/** @returns {Promise<void>} Checks the default 7-day table, totals and chart. */
async function checkSevenDays() {
  renderWithMantine(<EnergyPage />)
  expect(await screen.findByText('7.00 kWh')).toBeInTheDocument()
  expect(screen.getByText('£1.75')).toBeInTheDocument()
  expect(screen.getByText('1.45 kg')).toBeInTheDocument()
  expect(screen.getAllByRole('row')).toHaveLength(8)
  expect(BarChart.mock.lastCall[0].data).toHaveLength(7)
}
it('shows daily energy, cost and CO2 totals for seven days', checkSevenDays)

/** @returns {Promise<void>} Checks the range switch reloads thirty daily summaries. */
async function checkThirtyDays() {
  const user = userEvent.setup()
  renderWithMantine(<EnergyPage />)
  await user.click(screen.getByRole('radio', { name: '30 days' }))
  expect(await screen.findByText('30.00 kWh')).toBeInTheDocument()
  expect(screen.getAllByRole('row')).toHaveLength(31)
  expect(BarChart.mock.lastCall[0].data).toHaveLength(30)
}
it('switches to thirty days and updates the table and chart', checkThirtyDays)
