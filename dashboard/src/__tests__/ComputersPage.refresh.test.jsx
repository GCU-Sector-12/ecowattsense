// Page-level regression tests for saved computer data during automatic refresh.
import { act, fireEvent, screen } from '@testing-library/react'
import { MantineProvider } from '@mantine/core'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import ComputersPage from '../pages/ComputersPage.jsx'
import HistoryPage from '../pages/HistoryPage.jsx'
import ComputerEditModal from '../components/ComputerEditModal.jsx'
import RefreshStatus from '../components/RefreshStatus.jsx'
import { getDevices, getLocations, saveDevice } from '../data/index.js'
import { historyState, loadComputerHistory, rangeText, reportTime, shortDay } from '../data/historyView.js'
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
vi.mock('../components/EnergyChart.jsx', () => ({ default: () => null }))

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

/** @returns {object} A valid history response containing enough rows to test table paging. */
function makeHistory() {
  const reports = Array.from({ length: 30 }, (_, index) => ({
    timestamp: `2026-11-25T${String(Math.floor(index / 2)).padStart(2, '0')}:${index % 2 ? '30' : '00'}:00Z`,
    state: 2,
    cpu_percent: 10,
    idle_seconds: 60,
    on_battery: false,
    energy_uj: 100,
    interval_seconds: 1800,
  }))
  return {
    reports,
    totals: {
      active_time_seconds: 3600,
      idle_time_seconds: 0,
      unused_time_seconds: 0,
      energy_uj: 3000,
      cost_gbp: 0.01,
      co2_kg: 0.01,
    },
  }
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
  expect(refreshedRow.querySelector('[title]')).toHaveAttribute('title', refreshed.last_seen_at)
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
    <MantineProvider theme={theme} defaultColorScheme="light" env="test">
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

/** @returns {Promise<void>} Checks the refresh status displays both success and failure messages. */
async function showsRefreshStatusMessages() {
  const lastUpdatedAt = new Date('2026-11-25T21:00:00Z')
  const { rerender } = renderWithMantine(
    <RefreshStatus lastUpdatedAt={lastUpdatedAt} refreshError={null} />,
  )
  expect(screen.getByText(/^Updated /)).toBeInTheDocument()
  rerender(
    <MantineProvider theme={theme} defaultColorScheme="light" env="test">
      <RefreshStatus lastUpdatedAt={lastUpdatedAt} refreshError={new Error('offline')} />
    </MantineProvider>,
  )
  expect(screen.getByText(/^Could not update\. Last update /)).toBeInTheDocument()
}

/** @param {string} day - Selected calendar day. @returns {string} The unchanged day. */
function identityDay(day) {
  return day
}

/** @param {string} timestamp - Report timestamp. @returns {string} UTC hour and minute. */
function formatReportTime(timestamp) {
  return timestamp.slice(11, 16)
}

/** @param {number} page - Current page. @param {number} pageSize - Rows per page. @param {number} total - Report count. @returns {string} Visible report range. */
function formatRangeText(page, pageSize, total) {
  const first = total ? (page - 1) * pageSize + 1 : 0
  return `Showing ${first} to ${Math.min(page * pageSize, total)} of ${total} reports`
}

/** @returns {Promise<void>} Checks refresh keeps the selected day and current reports-table page. */
async function keepsHistoryDayAndTablePageAfterRefresh() {
  isRealHistory.mockReturnValue(true)
  loadComputerHistory.mockResolvedValue({
    device: initialDevice,
    history: makeHistory(),
  })
  historyState.mockReturnValue('active')
  reportTime.mockImplementation(formatReportTime)
  shortDay.mockImplementation(identityDay)
  rangeText.mockImplementation(formatRangeText)

  renderWithMantine(<HistoryPage deviceId="device-123" />)
  await act(async () => { await Promise.resolve(); await Promise.resolve() })

  const dateInput = screen.getByLabelText('History date')
  expect(dateInput).toHaveValue('2026-11-25')
  fireEvent.click(screen.getAllByRole('button', { name: '2' })[0])
  expect(screen.getAllByText('Showing 26 to 30 of 30 reports')).toHaveLength(2)

  await act(async () => { await vi.advanceTimersByTimeAsync(60000) })

  expect(screen.getByLabelText('History date')).toHaveValue('2026-11-25')
  expect(screen.getAllByText('Showing 26 to 30 of 30 reports')).toHaveLength(2)
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
  it('shows the last update and failed refresh messages', showsRefreshStatusMessages)
  it('keeps the selected history day and table page after refresh', keepsHistoryDayAndTablePageAfterRefresh)
})
