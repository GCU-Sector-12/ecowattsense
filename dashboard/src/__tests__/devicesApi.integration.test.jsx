// Real-mode device integration for unchanged pages. Run with npm test.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import ComputersPage from '../pages/ComputersPage.jsx'
import DashboardPage from '../pages/DashboardPage.jsx'
import { renderWithMantine } from '../test/render.jsx'
import sample from '../data/devices.json'

const liveDevice = { ...sample.devices[0], device_id: 'live-device',
  display_name: 'Live computer', state: null, state_name: 'no_data',
  location_id: null, location_name: null, building: null }
const archivedDevice = { ...liveDevice, device_id: 'archived-device',
  display_name: 'Archived computer', archived: true }

/** @returns {void} Selects real devices while the other endpoints remain sample. */
function prepare() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
    ok: true, json: async () => ({ devices: [liveDevice, archivedDevice] }),
  }))
}

/** @returns {void} Restores request globals and modes. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {Promise<void>} Checks state words and the existing local archive switch. */
async function checkComputers() {
  const user = userEvent.setup()
  renderWithMantine(<ComputersPage />)
  const cell = await screen.findByRole('cell', { name: 'Live computer' })
  expect(within(cell.closest('tr')).getByRole('cell', { name: 'No data' })).toBeInTheDocument()
  expect(screen.queryByRole('cell', { name: 'Archived computer' })).not.toBeInTheDocument()
  await user.click(screen.getByRole('switch', { name: 'Show archived' }))
  expect(screen.getByRole('cell', { name: 'Archived computer' })).toBeInTheDocument()
  expect(fetch).toHaveBeenCalledTimes(1)
}
it('displays API device names, state words and archived rows', checkComputers)

/** @returns {Promise<void>} Checks the Dashboard reads the same real provider without page changes. */
async function checkDashboard() {
  renderWithMantine(<DashboardPage />)
  expect(await screen.findByRole('cell', { name: 'Live computer' })).toBeInTheDocument()
  expect(fetch).toHaveBeenCalledWith('/api/devices?include_archived=true', {
    method: 'GET', credentials: 'include',
  })
}
it('uses real devices on the Dashboard too', checkDashboard)

/** @param {number} status - HTTP failure. @returns {Promise<void>} Checks the page exposes the failure. */
async function checkPageError(status) {
  fetch.mockResolvedValue({ ok: false, status })
  renderWithMantine(<ComputersPage />)
  expect(await screen.findByRole('alert')).toBeInTheDocument()
  expect(screen.queryByRole('cell', { name: 'PC-01' })).not.toBeInTheDocument()
}
it.each([401, 422])('shows a page error for HTTP %s', checkPageError)
