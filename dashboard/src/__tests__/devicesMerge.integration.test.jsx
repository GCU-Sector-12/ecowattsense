// Merge regression tests for real devices, request states and last-seen displays.
// Run with npm test to check WEB-36 and WEB-38 together on both device pages.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { act, screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import ComputersPage from '../pages/ComputersPage.jsx'
import DashboardPage from '../pages/DashboardPage.jsx'
import { renderWithMantine } from '../test/render.jsx'
import sample from '../data/devices.json'

const pages = [
  { Page: ComputersPage, label: 'computers' },
  { Page: DashboardPage, label: 'dashboard' },
]
const currentTime = new Date('2026-11-12T11:00:00Z')
const liveDevice = {
  ...sample.devices[0], device_id: 'merge-test-device', display_name: 'Live computer',
  last_seen_at: '2026-11-12T10:58:00Z',
}

/** @returns {void} Isolates real requests and fixes only Date so browser timers keep running. */
function prepare() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue(deviceResponse()))
  vi.useFakeTimers({ toFake: ['Date'] })
  vi.setSystemTime(currentTime)
  window.history.replaceState(null, '', '/')
}

/** @returns {void} Restores request globals, clock, modes and development switches. */
function restore() {
  vi.useRealTimers()
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
  window.history.replaceState(null, '', '/')
}
beforeEach(prepare)
afterEach(restore)

/** @param {object[]} [devices] - API rows. @returns {object} Successful mock API response. */
function deviceResponse(devices = [liveDevice]) {
  return { ok: true, json: async () => ({ devices }) }
}

/**
 * Checks that real devices are timed against the current clock instead of the sample date.
 * @param {object} page - Page component.
 * @returns {Promise<void>} Resolves after the API row is displayed.
 */
async function checkRealLastSeen({ Page }) {
  renderWithMantine(<Page />)
  const name = await screen.findByRole('cell', { name: 'Live computer' })
  expect(within(name.closest('tr')).getByRole('cell', { name: '2 mins ago' })).toBeInTheDocument()
}
it.each(pages)('keeps current-time Last seen on $label', checkRealLastSeen)

/**
 * Checks sample displays keep their fixed reference date even when the real clock changes.
 * @param {object} page - Page component.
 * @returns {Promise<void>} Resolves after the sample row is displayed.
 */
async function checkSampleLastSeen({ Page }) {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'sample')
  renderWithMantine(<Page />)
  const name = await screen.findByRole('cell', { name: 'PC-01' })
  expect(within(name.closest('tr')).getByRole('cell', { name: '2 mins ago' })).toBeInTheDocument()
  expect(fetch).not.toHaveBeenCalled()
}
it.each(pages)('keeps the sample Last seen reference on $label', checkSampleLastSeen)

/**
 * Checks a real request failure can retry through a loading skeleton and recover.
 * @param {object} page - Page component and accessible loading label.
 * @returns {Promise<void>} Resolves after retry succeeds.
 */
async function checkRealRetry({ Page, label }) {
  const user = userEvent.setup()
  fetch.mockResolvedValueOnce({ ok: false, status: 500 })
  renderWithMantine(<Page />)
  expect(await screen.findByRole('alert')).toHaveTextContent('Could not load data. Try again.')
  expect(screen.queryByRole('cell', { name: 'PC-01' })).not.toBeInTheDocument()
  const pending = Promise.withResolvers()
  fetch.mockReturnValueOnce(pending.promise)
  await user.click(screen.getByRole('button', { name: 'Retry' }))
  expect(screen.getByRole('status', { name: 'Loading ' + label })).toBeInTheDocument()
  expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  await act(() => pending.resolve(deviceResponse()))
  expect(await screen.findByRole('cell', { name: 'Live computer' })).toBeInTheDocument()
  expect(fetch).toHaveBeenCalledTimes(2)
}
it.each(pages)('retries a real API failure through the $label loading state', checkRealRetry)

/**
 * Checks an empty real response shows the WEB-36 empty state without fixture fallback.
 * @param {object} page - Page component.
 * @returns {Promise<void>} Resolves after the empty response is displayed.
 */
async function checkRealEmpty({ Page }) {
  fetch.mockResolvedValue(deviceResponse([]))
  renderWithMantine(<Page />)
  expect(await screen.findByRole('status', { name: 'Computers' })).toHaveTextContent('No data')
  expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  expect(screen.queryByRole('table')).not.toBeInTheDocument()
}
it.each(pages)('keeps the real $label empty state', checkRealEmpty)
