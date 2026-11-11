// Real summary integration for the unchanged Dashboard. Run with npm test.
// Chart rendering is replaced with spies so values can be checked without browser sizing.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { screen } from '@testing-library/react'
import { AreaChart, DonutChart } from '@mantine/charts'
import DashboardPage from '../pages/DashboardPage.jsx'
import { renderWithMantine } from '../test/render.jsx'
import sample from '../data/summary.json'

vi.mock('@mantine/charts', () => ({
  AreaChart: vi.fn(() => <div data-testid="power-chart" />),
  DonutChart: vi.fn(() => <div data-testid="status-chart" />),
}))

const response = { ...sample, date: '2026-07-01', energy_uj: 48600000000000,
  cost_gbp: 3.38, co2_kg: 2.79,
  counts: { active: 2, idle: 1, unused: 0, no_data: 1, total: 4 },
  series: [{ timestamp: '2026-06-30T23:00:00Z', energy_uj: 900000000000 }] }

/** @returns {void} Selects real summary with a controlled API response. */
function prepare() {
  vi.clearAllMocks()
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: true, json: async () => response }))
}

/** @returns {void} Restores globals and endpoint mode. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {Promise<void>} Checks API totals, state counts and the chart's existing conversion. */
async function checkDashboard() {
  renderWithMantine(<DashboardPage />)
  expect(await screen.findByText('13.5 kWh')).toBeInTheDocument()
  expect(screen.getByText('£3.38')).toBeInTheDocument()
  expect(screen.getByLabelText('Active Computers: 50%')).toBeInTheDocument()
  expect(screen.getByLabelText('4 computers in total')).toBeInTheDocument()
  expect(AreaChart.mock.calls.at(-1)[0].data).toEqual([
    expect.objectContaining({ timestamp: '2026-06-30T23:00:00Z', power_kw: 0.5, energy_kwh: 0.25 }),
  ])
  expect(DonutChart.mock.calls.at(-1)[0].data.map(point => point.value)).toEqual([2, 1, 0, 1])
  expect(fetch).toHaveBeenCalledWith('/api/summary', { method: 'GET', credentials: 'include' })
}
it('displays real totals, counts and chart power', checkDashboard)

/** @param {number} status - HTTP failure. @returns {Promise<void>} Checks the page exposes API failures. */
async function checkError(status) {
  fetch.mockResolvedValue({ ok: false, status })
  renderWithMantine(<DashboardPage />)
  expect(await screen.findByRole('alert')).toBeInTheDocument()
  expect(screen.queryByText('13.5 kWh')).not.toBeInTheDocument()
}
it.each([401, 422])('shows an error for summary HTTP %s', checkError)
