// Real-mode computer editing integration tests for EWS_WEB-56.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import ComputersPage from '../pages/ComputersPage.jsx'
import { renderWithMantine } from '../test/render.jsx'
import sample from '../data/devices.json'

const computer = { ...sample.devices[0], display_name: 'PC-01', location_id: null,
  location_name: null, building: null, archived: false }
const room = { id: 7, name: 'Lab 1', building: 'George Moore' }

/** @returns {void} Selects the real API and supplies the initial device response. */
function prepare() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn().mockImplementation((url, options = {}) => {
    if (url === '/api/devices?include_archived=true') {
      return Promise.resolve({ ok: true, json: async () => ({ devices: [computer] }) })
    }
    if (url === '/api/locations') {
      return Promise.resolve({ ok: true, json: async () => ({ locations: [room] }) })
    }
    return Promise.resolve({ ok: false, status: 500 })
  }))
}

/** @returns {void} Restores mocked browser globals and environment. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {Promise<void>} Edits the name and room and checks the table without reloading. */
async function savesComputer() {
  const user = userEvent.setup()
  const updated = {
    ...computer, display_name: 'Renamed PC', location_id: room.id,
    location_name: room.name, building: room.building,
  }
  fetch.mockImplementation((url, options = {}) => {
    if (url === '/api/devices?include_archived=true') {
      return Promise.resolve({ ok: true, json: async () => ({ devices: [computer] }) })
    }
    if (url === '/api/locations') {
      return Promise.resolve({ ok: true, json: async () => ({ locations: [room] }) })
    }
    return Promise.resolve({ ok: true, json: async () => updated })
  })
  renderWithMantine(<ComputersPage />)
  await screen.findByRole('cell', { name: 'PC-01' })
  await user.click(screen.getByRole('button', { name: 'Edit PC-01' }))
  const name = await screen.findByRole('textbox', { name: 'Computer name' })
  await user.clear(name)
  await user.type(name, 'Renamed PC')
  const location = screen.getByRole('combobox', { name: 'Location' })
  await waitFor(() => expect(fetch).toHaveBeenCalledWith('/api/locations', {
    method: 'GET', credentials: 'include',
  }))
  await waitFor(() => expect(location).toBeEnabled())
  await user.click(location)
  await user.type(location, 'Lab 1')
  await user.keyboard('{ArrowDown}{Enter}')
  expect(location).toHaveValue('Lab 1 — George Moore')
  await user.click(screen.getByRole('button', { name: 'Save changes' }))
  expect(await screen.findByRole('cell', { name: 'Renamed PC' })).toBeInTheDocument()
  expect(fetch).toHaveBeenCalledWith('/api/admin/devices/' + computer.device_id, {
    method: 'PATCH', credentials: 'include',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ display_name: 'Renamed PC', location_id: room.id }),
  })
}
it('saves a computer name and location and updates the table without reloading', savesComputer)

/** @param {number} status - Rejected update status. @returns {Promise<void>} Checks visible error. */
async function showsSaveError(status) {
  const user = userEvent.setup()
  fetch.mockImplementation((url) => {
    if (url === '/api/devices?include_archived=true') {
      return Promise.resolve({ ok: true, json: async () => ({ devices: [computer] }) })
    }
    if (url === '/api/locations') {
      return Promise.resolve({ ok: true, json: async () => ({ locations: [] }) })
    }
    return Promise.resolve({ ok: false, status })
  })
  renderWithMantine(<ComputersPage />)
  await screen.findByRole('cell', { name: 'PC-01' })
  await user.click(screen.getByRole('button', { name: 'Edit PC-01' }))
  await screen.findByRole('textbox', { name: 'Computer name' })
  await user.clear(screen.getByRole('textbox', { name: 'Computer name' }))
  await user.type(screen.getByRole('textbox', { name: 'Computer name' }), 'Changed')
  await user.click(screen.getByRole('button', { name: 'Save changes' }))
  expect(await screen.findByRole('alert')).toHaveTextContent(
    status === 422 ? 'details were not accepted' : 'could not be found',
  )
}
it.each([422, 404])('shows a useful message when saving returns HTTP %s', showsSaveError)
