// Request-state coverage for Dashboard, Computers and Settings. Run with npm test.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { act, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import DashboardPage from '../pages/DashboardPage.jsx'
import ComputersPage from '../pages/ComputersPage.jsx'
import SettingsPage from '../pages/SettingsPage.jsx'
import * as providers from '../data/index.js'
import { renderWithMantine } from '../test/render.jsx'

/** @returns {void} Gives each test an isolated development URL. */
function prepare() {
  vi.stubEnv('DEV', true)
  window.history.replaceState(null, '', '/')
}

/** @returns {void} Restores real providers, timers and URL switches. */
function restore() {
  vi.restoreAllMocks()
  vi.useRealTimers()
  vi.unstubAllEnvs()
  window.history.replaceState(null, '', '/')
}
beforeEach(prepare)
afterEach(restore)

const pages = [
  { Page: DashboardPage, label: 'dashboard', content: 'Welcome to EcoWattSense!' },
  { Page: ComputersPage, label: 'computers', content: 'PC-01' },
  { Page: SettingsPage, label: 'settings', content: /Report interval:/ },
]

/**
 * Checks that the development delay shows a skeleton until providers resolve.
 * @param {object} page - Component, accessible loading label and resolved content.
 * @returns {Promise<void>} Resolves after the data arrives.
 */
async function checkLoading({ Page, label, content }) {
  vi.useFakeTimers()
  window.history.replaceState(null, '', '/?delay=2000')
  renderWithMantine(<Page />)
  expect(screen.getByRole('status', { name: 'Loading ' + label })).toBeInTheDocument()
  await act(() => vi.advanceTimersByTimeAsync(1999))
  expect(screen.getByRole('status', { name: 'Loading ' + label })).toBeInTheDocument()
  await act(() => vi.advanceTimersByTimeAsync(1))
  expect(screen.queryByRole('status', { name: 'Loading ' + label })).not.toBeInTheDocument()
  expect(screen.getByText(content)).toBeInTheDocument()
}
it.each(pages)('shows skeletons during a delayed $label request', checkLoading)

/**
 * Checks failure, a repeated failure and successful recovery without remounting.
 * @param {object} page - Component and resolved content.
 * @returns {Promise<void>} Resolves after retry succeeds.
 */
async function checkRetry({ Page, content }) {
  const user = userEvent.setup()
  window.history.replaceState(null, '', '/?fail=1')
  renderWithMantine(<Page />)
  expect(await screen.findByRole('alert')).toHaveTextContent('Could not load data. Try again.')
  await user.click(screen.getByRole('button', { name: 'Retry' }))
  expect(await screen.findByRole('alert')).toHaveTextContent('Could not load data. Try again.')
  window.history.replaceState(null, '', '/')
  await user.click(screen.getByRole('button', { name: 'Retry' }))
  expect(await screen.findByText(content)).toBeInTheDocument()
  expect(screen.queryByRole('alert')).not.toBeInTheDocument()
}
it.each(pages)('retries a failed $label request', checkRetry)

/** @returns {Promise<void>} Checks that a successful empty device list has its own card. */
async function checkEmptyComputers() {
  vi.spyOn(providers, 'getDevices').mockResolvedValue({ devices: [] })
  renderWithMantine(<ComputersPage />)
  expect(await screen.findByRole('status', { name: 'Computers' })).toHaveTextContent('No data')
  expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  expect(screen.queryByRole('table')).not.toBeInTheDocument()
}
it('shows No data for an empty Computers response', checkEmptyComputers)

/** @returns {Promise<void>} Checks empty lists and series without hiding valid zero totals. */
async function checkEmptyDashboard() {
  const summary = await providers.getSummary()
  vi.spyOn(providers, 'getDevices').mockResolvedValue({ devices: [] })
  vi.spyOn(providers, 'getSummary').mockResolvedValue({
    ...summary, counts: { active: 0, idle: 0, unused: 0, no_data: 0, total: 0 },
    energy_uj: 0, cost_gbp: 0, co2_kg: 0, series: [],
  })
  renderWithMantine(<DashboardPage />)
  expect(await screen.findByRole('status', { name: 'Computers' })).toHaveTextContent('No data')
  expect(screen.getByRole('status', { name: 'Energy usage' })).toHaveTextContent('No data')
  expect(screen.getByRole('status', { name: 'Computer status' })).toHaveTextContent('No data')
  expect(screen.getByText('0.0 kWh')).toBeInTheDocument()
}
it('shows No data for empty Dashboard regions', checkEmptyDashboard)

/** @returns {Promise<void>} Checks that filters keep working after an empty search result. */
async function checkEmptySearch() {
  const user = userEvent.setup()
  renderWithMantine(<ComputersPage />)
  await screen.findByText('PC-01')
  await user.type(screen.getByRole('textbox', { name: 'Search computers' }), 'unknown computer')
  expect(screen.getByRole('status', { name: 'Computers' })).toHaveTextContent('No data')
  await user.clear(screen.getByRole('textbox', { name: 'Search computers' }))
  await waitFor(() => expect(screen.getByText('PC-01')).toBeInTheDocument())
}
it('can clear a search with no matches', checkEmptySearch)
