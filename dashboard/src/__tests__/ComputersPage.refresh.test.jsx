// Page-level regression tests for saved computer data during automatic refresh.
import { act, fireEvent, screen } from '@testing-library/react'
import { MantineProvider } from '@mantine/core'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import ComputersPage from '../pages/ComputersPage.jsx'
import HistoryPage from '../pages/HistoryPage.jsx'
import ComputerEditModal from '../components/ComputerEditModal.jsx'
import { getDevices, getLocations, saveDevice } from '../data/index.js'
import { loadComputerHistory } from '../data/historyView.js'
import { isRealHistory } from '../data/historyProvider.js'
import { renderWithMantine } from '../test/render.jsx'
import { theme } from '../theme.js'

vi.mock('../data/index.js', () => ({
  getDevices: vi.fn(),
  getLocations: vi.fn(),
  saveDevice: vi.fn(),
}))
vi.mock('../data/historyView.js', () => ({
  calendarDay: () => '2026-11-25',
  energyByHalfHour: vi.fn(),
  formatDuration: vi.fn(),
  historyDate: vi.fn(),
  historyState: vi.fn(),
  loadComputerHistory: vi.fn(),
  rangeText: vi.fn(),
  reportTime: vi.fn(),
  SAMPLE_HISTORY_FROM: '2025-03-10T00:00:00Z',
  shiftDay: day => day,
  shortDay: vi.fn(),
  timelinePlace: vi.fn(),
}))
vi.mock('../data/historyProvider.js', () => ({ isRealHistory: vi.fn() }))

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

  renderWithMantine(<ComputersPage />)
  await act(async () => { await Promise.resolve(); await Promise.resolve() })

  fireEvent.click(screen.getByRole('button', { name: 'Edit Office PC' }))
  const nameInput = await screen.findByLabelText('Computer name')
  fireEvent.change(nameInput, { target: { value: 'Renamed PC' } })
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

/** @returns {Promise<void>} Checks a refreshed computer object does not reset an open edit form. */
async function preservesOpenEditInputs() {
  getLocations.mockResolvedValue({ locations: [] })
  const onClose = vi.fn()
  const onSaved = vi.fn()
  const { rerender } = renderWithMantine(
    <ComputerEditModal computer={initialDevice} opened onClose={onClose} onSaved={onSaved} />,
  )
  const nameInput = await screen.findByLabelText('Computer name')
  fireEvent.change(nameInput, { target: { value: 'My in-progress name' } })
  rerender(
    <MantineProvider theme={theme} defaultColorScheme="light">
      <ComputerEditModal computer={makeDevice({ display_name: 'Server name' })}
        opened onClose={onClose} onSaved={onSaved} />
    </MantineProvider>,
  )
  expect(screen.getByLabelText('Computer name')).toHaveValue('My in-progress name')
}

/** @returns {Promise<void>} Checks HistoryPage replaces a saved device snapshot after refresh. */
async function refreshesSavedHistoryDevice() {
  const saved = makeDevice({ display_name: 'Renamed PC' })
  const refreshed = makeDevice({
    display_name: 'Renamed PC',
    last_seen_at: '2026-11-25T21:00:00Z',
    location_name: 'New room',
  })
  isRealHistory.mockReturnValue(true)
  loadComputerHistory.mockResolvedValueOnce({ device: initialDevice, history: null })
    .mockResolvedValueOnce({ device: refreshed, history: null })
  getLocations.mockResolvedValue({ locations: [] })
  saveDevice.mockResolvedValue(saved)

  renderWithMantine(<HistoryPage deviceId="device-123" />)
  await act(async () => { await Promise.resolve(); await Promise.resolve() })

  fireEvent.click(screen.getByRole('button', { name: 'Edit computer' }))
  const nameInput = await screen.findByLabelText('Computer name')
  fireEvent.change(nameInput, { target: { value: 'Renamed PC' } })
  await act(async () => {
    fireEvent.click(screen.getByRole('button', { name: 'Save changes' }))
    await Promise.resolve()
    await Promise.resolve()
  })

  expect(screen.getByText('Renamed PC')).toBeInTheDocument()
  await act(async () => { await vi.advanceTimersByTimeAsync(60000) })
  expect(screen.getByTitle(refreshed.last_seen_at)).toBeInTheDocument()
  expect(screen.getByText('New room')).toBeInTheDocument()
  expect(loadComputerHistory).toHaveBeenCalledTimes(2)
}

beforeEach(() => {
  vi.resetAllMocks()
  vi.useFakeTimers({ toFake: ['setInterval', 'clearInterval'] })
  Object.defineProperty(document, 'visibilityState', { configurable: true, value: 'visible' })
})
afterEach(() => vi.useRealTimers())

describe('ComputersPage automatic refresh', () => {
  it('shows fresh state and last-seen values after a computer is saved', refreshesSavedComputerFromServer)
  it('preserves form inputs when refreshed device details change', preservesOpenEditInputs)
  it('refreshes saved device details on the History page', refreshesSavedHistoryDevice)
})
