// Page-level regression tests for saved computer data during automatic refresh.
import { act, fireEvent, render, screen } from '@testing-library/react'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import ComputersPage from '../pages/ComputersPage.jsx'
import { getDevices, getLocations, saveDevice } from '../data/index.js'

vi.mock('../data/index.js', () => ({
  getDevices: vi.fn(),
  getLocations: vi.fn(),
  saveDevice: vi.fn(),
}))

const initialDevice = {
  device_id: 'device-123',
  display_name: 'Office PC',
  state_name: 'active',
  last_seen_at: '2026-11-25T20:00:00Z',
  location_id: null,
  location_name: null,
  building: null,
  os: 'Windows 11',
  device_type: 'Desktop',
  archived: false,
}

/** @returns {object} A device fixture with the supplied field changes. */
function makeDevice(changes) {
  return { ...initialDevice, ...changes }
}

/** @returns {Promise<void>} Checks a saved row yields to the next successful server refresh. */
async function refreshesSavedComputerFromServer() {
  const saved = makeDevice({ display_name: 'Renamed PC' })
  const refreshed = makeDevice({
    display_name: 'Renamed PC',
    state_name: 'no_data',
    last_seen_at: '2026-11-25T21:00:00Z',
  })
  getDevices.mockResolvedValueOnce({ devices: [initialDevice] })
    .mockResolvedValueOnce({ devices: [refreshed] })
  getLocations.mockResolvedValue({ locations: [] })
  saveDevice.mockResolvedValue(saved)

  render(<ComputersPage />)
  await act(async () => { await Promise.resolve(); await Promise.resolve() })

  fireEvent.click(screen.getByRole('button', { name: 'Edit Office PC' }))
  fireEvent.change(screen.getByLabelText('Computer name'), { target: { value: 'Renamed PC' } })
  await act(async () => {
    fireEvent.click(screen.getByRole('button', { name: 'Save changes' }))
    await Promise.resolve()
    await Promise.resolve()
  })

  expect(screen.getByRole('row', { name: /Renamed PC/ })).toBeInTheDocument()
  const row = screen.getByRole('row', { name: /Renamed PC/ })
  expect(row).toHaveTextContent('Active')

  await act(async () => { await vi.advanceTimersByTimeAsync(60000) })

  const refreshedRow = screen.getByRole('row', { name: /Renamed PC/ })
  expect(refreshedRow).toHaveTextContent('No data')
  expect(refreshedRow.querySelector('time')).toHaveAttribute('title', refreshed.last_seen_at)
  expect(getDevices).toHaveBeenCalledTimes(2)
}

beforeEach(() => {
  vi.clearAllMocks()
  vi.useFakeTimers({ toFake: ['setInterval', 'clearInterval'] })
})
afterEach(() => vi.useRealTimers())

describe('ComputersPage automatic refresh', () => {
  it('shows fresh state and last-seen values after a computer is saved', refreshesSavedComputerFromServer)
})
