import { beforeEach, expect, it, vi } from 'vitest'
import { screen } from '@testing-library/react'
import DashboardPage from '../pages/DashboardPage.jsx'
import { getDevices, getSettings, getSummary } from '../data/index.js'
import { renderWithMantine } from '../test/render.jsx'

vi.mock('../data/index.js', () => ({
  getSummary: vi.fn(),
  getDevices: vi.fn(),
  getSettings: vi.fn(),
}))

const summary = {
  counts: { active: 12, idle: 5, unused: 2, no_data: 1, total: 20 },
  energy_uj: 0, cost_gbp: 0, co2_kg: 0,
  price_gbp_per_kwh: 0.25, co2_kg_per_kwh: 0.207, series: [],
}

beforeEach(() => {
  vi.clearAllMocks()
  getSummary.mockResolvedValue(summary)
  getDevices.mockResolvedValue({ devices: [] })
  getSettings.mockResolvedValue({
    idle_threshold_seconds: 900,
    unused_threshold_seconds: 7200,
  })
})

it('shows state counts, share bars and the singular no-data count', async () => {
  renderWithMantine(<DashboardPage />)
  expect(await screen.findByText('1 computer with no data')).toBeInTheDocument()
  expect(screen.getByLabelText('Active Computers: 60%')).toBeInTheDocument()
  expect(screen.getByLabelText('Idle Computers: 25%')).toBeInTheDocument()
  expect(screen.getByLabelText('Unused Computers: 10%')).toBeInTheDocument()
})

it('derives idle and unused captions from the returned settings', async () => {
  renderWithMantine(<DashboardPage />)
  expect(await screen.findByText('No activity for 15+ mins')).toBeInTheDocument()
  expect(screen.getByText('No activity for 2+ hours')).toBeInTheDocument()
})

it('uses plural wording when more than one computer has no data', async () => {
  getSummary.mockResolvedValue({
    ...summary, counts: { ...summary.counts, no_data: 2 },
  })
  renderWithMantine(<DashboardPage />)
  expect(await screen.findByText('2 computers with no data')).toBeInTheDocument()
})
