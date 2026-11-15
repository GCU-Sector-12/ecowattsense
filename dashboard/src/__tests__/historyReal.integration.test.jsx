// HistoryPage in real mode: date selection, chart, empty day, 404 and 401, with fetch stubbed.
import { fireEvent, screen, waitFor, within } from '@testing-library/react'
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import HistoryPage from '../pages/HistoryPage.jsx'
import { renderWithMantine } from '../test/render.jsx'
import devices from '../data/devices.json'

const device = { ...devices.devices[0], device_id: 'real-1', display_name: 'Real PC' }
const totals = {
  active_time_seconds: 600, idle_time_seconds: 300, unused_time_seconds: 0,
  energy_uj: 7200000000000, cost_gbp: 0.0123, co2_kg: 0.0456,
}

/** @param {string} day - YYYY-MM-DD. @param {number} count - Reports, 300 s apart. @returns {object} History body. */
function historyFor(day, count) {
  const start = Date.parse(day + 'T00:00:00Z')
  const reports = Array.from({ length: count }, (_, i) => ({
    timestamp: new Date(start + i * 300000).toISOString(), interval_seconds: 300, state: 2,
    idle_seconds: 0, cpu_percent: 12.5, screen_locked: false, on_battery: false,
    energy_uj: 1000000, energy_source: 'estimated',
  }))
  return { device_id: 'real-1', display_name: 'Real PC', from: day, to: day, reports, totals }
}

/** @param {object} history - History body. @param {number} [status] - History status. @returns {Function} Router for fetch. */
function route(history, status = 200) {
  return async url => {
    if (String(url).startsWith('/api/devices?')) return { ok: true, status: 200, json: async () => ({ devices: [device] }) }
    return { ok: status === 200, status, json: async () => history }
  }
}

/** @returns {void} Selects real mode for devices and history. */
function prepare() {
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  vi.stubEnv('VITE_HISTORY_DATA_SOURCE', 'real')
}

/** @returns {void} Restores globals and modes. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {string[]} History request URLs seen by fetch. */
function historyUrls() {
  return fetch.mock.calls.map(call => String(call[0])).filter(url => url.includes('/history'))
}

it('loads the selected UTC day, shows 288 blocks, the chart and the response totals', async () => {
  vi.stubGlobal('fetch', vi.fn(route(historyFor('2026-11-14', 288))))
  renderWithMantine(<HistoryPage deviceId="real-1" />)
  fireEvent.change(screen.getByLabelText('History date'), { target: { value: '2026-11-14' } })
  const list = await screen.findByRole('list', { name: 'Daily activity reports' })
  await waitFor(() => expect(within(list).getAllByRole('listitem')).toHaveLength(288))
  const url = historyUrls().at(-1)
  expect(url).toContain('from=2026-11-14T00%3A00%3A00Z')
  expect(url).toContain('to=2026-11-15T00%3A00%3A00Z')
  expect(screen.getByText('2.000 kWh')).toBeInTheDocument()
  expect(screen.getByText('Energy usage, 14 Nov 2026')).toBeInTheDocument()
  expect(screen.queryByText(/Sample history/)).not.toBeInTheDocument()
  expect(screen.queryByText(/Date selection does not change/)).not.toBeInTheDocument()
})

it('loads another day when the date changes', async () => {
  vi.stubGlobal('fetch', vi.fn(route(historyFor('2026-11-13', 3))))
  renderWithMantine(<HistoryPage deviceId="real-1" />)
  await screen.findByRole('list', { name: 'Daily activity reports' })
  const before = historyUrls().length
  fireEvent.change(screen.getByLabelText('History date'), { target: { value: '2026-11-13' } })
  await waitFor(() => expect(historyUrls().length).toBeGreaterThan(before))
  expect(historyUrls().at(-1)).toContain('from=2026-11-13T00%3A00%3A00Z')
})

it('shows the empty state for a day without reports', async () => {
  vi.stubGlobal('fetch', vi.fn(route(historyFor('2026-11-14', 0))))
  renderWithMantine(<HistoryPage deviceId="real-1" />)
  expect(await screen.findByRole('status', { name: 'History reports' })).toBeInTheDocument()
  expect(screen.queryByRole('alert')).not.toBeInTheDocument()
})

it('shows Device not found for HTTP 404', async () => {
  vi.stubGlobal('fetch', vi.fn(route({ detail: 'Device not found' }, 404)))
  renderWithMantine(<HistoryPage deviceId="real-1" />)
  expect(await screen.findByRole('heading', { name: 'Device not found' })).toBeInTheDocument()
})

it('shows the retry error for HTTP 401, like the devices page', async () => {
  vi.stubGlobal('fetch', vi.fn(route({}, 401)))
  renderWithMantine(<HistoryPage deviceId="real-1" />)
  expect(await screen.findByRole('alert')).toBeInTheDocument()
})

it('shows the current time in the header, not the sample time', async () => {
  vi.stubGlobal('fetch', vi.fn(route(historyFor('2026-11-14', 3))))
  renderWithMantine(<HistoryPage deviceId="real-1" />)
  await screen.findByRole('list', { name: 'Daily activity reports' })
  expect(screen.queryByText(/10 Mar 2025/)).not.toBeInTheDocument()
})

it('pages the table, keeps the size and goes back to page 1 on a day change', async () => {
  vi.stubGlobal('fetch', vi.fn(route(historyFor('2026-11-14', 60))))
  renderWithMantine(<HistoryPage deviceId="real-1" />)
  fireEvent.change(screen.getByLabelText('History date'), { target: { value: '2026-11-14' } })
  const table = await screen.findByRole('table', { name: 'History reports' })
  expect(within(table).getAllByRole('row')).toHaveLength(26)
  expect(within(screen.getByRole('list', { name: 'Daily activity reports' })).getAllByRole('listitem')).toHaveLength(60)
  fireEvent.click(screen.getByRole('button', { name: /next page/i }))
  expect(screen.getByText('Showing 26 to 50 of 60 reports')).toBeInTheDocument()
  fireEvent.click(screen.getByRole('radio', { name: '50' }))
  expect(screen.getByText('Showing 1 to 50 of 60 reports')).toBeInTheDocument()
  fireEvent.click(screen.getByRole('button', { name: /next page/i }))
  fireEvent.change(screen.getByLabelText('History date'), { target: { value: '2026-11-13' } })
  expect(await screen.findByText('Showing 1 to 50 of 60 reports')).toBeInTheDocument()
})
