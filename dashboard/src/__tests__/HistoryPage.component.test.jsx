// HistoryPage acceptance checks using the real sample providers and controlled failures.
import { fireEvent, screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { vi } from 'vitest'
import { MantineProvider } from '@mantine/core'
import { theme } from '../theme.js'
import HistoryPage from '../pages/HistoryPage.jsx'
import { renderWithMantine } from '../test/render.jsx'
import history from '../data/history.json'
import devices from '../data/devices.json'
import * as providers from '../data/index.js'
import { calendarDay } from '../data/historyView.js'

/** @returns {void} Restores provider spies between independent acceptance cases. */
function restoreProviders() {
  vi.restoreAllMocks()
}
afterEach(restoreProviders)

it('shows device identity and correctly formatted totals for the sample computer', async () => {
  renderWithMantine(<HistoryPage deviceId={history.device_id} />)
  expect(await screen.findByRole('heading', { name: 'PC-01' })).toBeInTheDocument()
  expect(screen.getByText(history.device_id, { exact: true })).toBeInTheDocument()
  expect(screen.getByText('windows', { exact: true })).toBeInTheDocument()
  expect(screen.getByText('laptop', { exact: true })).toBeInTheDocument()
  expect(screen.getByText('Not set')).toBeInTheDocument()
  expect(screen.getByText('2 mins ago')).toBeInTheDocument()
  expect(within(screen.getByRole('region', { name: 'Active time' })).getByText('10 h 0 min')).toBeInTheDocument()
  expect(within(screen.getByRole('region', { name: 'Idle time' })).getByText('6 h 0 min')).toBeInTheDocument()
  expect(within(screen.getByRole('region', { name: 'Unused time' })).getByText('8 h 0 min')).toBeInTheDocument()
  expect(within(screen.getByRole('region', { name: 'Energy' })).getByText('18.330 kWh')).toBeInTheDocument()
  expect(within(screen.getByRole('region', { name: 'Cost' })).getByText('£4.58')).toBeInTheDocument()
  expect(within(screen.getByRole('region', { name: 'CO₂' })).getByText('3.79 kg')).toBeInTheDocument()
})

it('shows 48 focusable timeline blocks and readable state words in all report rows', async () => {
  renderWithMantine(<HistoryPage deviceId={history.device_id} />)
  const table = await screen.findByRole('table', { name: 'History reports' })
  const rows = within(table).getAllByRole('row')
  expect(rows).toHaveLength(49)
  expect(within(rows[1]).getByText('00:00')).toBeInTheDocument()
  expect(within(rows[1]).getByText('Unused')).toBeInTheDocument()
  expect(within(rows[1]).getByText('1.1%')).toBeInTheDocument()
  expect(within(rows[1]).getByText('1 h 5 min')).toBeInTheDocument()
  expect(within(rows[1]).getByText('No')).toBeInTheDocument()
  expect(within(rows[1]).getByText('0.150 kWh')).toBeInTheDocument()
  expect(within(table).getAllByText('Active')).toHaveLength(20)
  expect(within(table).getAllByText('Idle')).toHaveLength(12)
  expect(within(table).getAllByText('Unused')).toHaveLength(16)
  const blocks = within(screen.getByRole('list', { name: 'Daily activity reports' })).getAllByRole('listitem')
  expect(blocks).toHaveLength(48)
  expect(blocks[0]).toHaveAttribute('aria-label', '00:00 UTC — Unused')
  expect(blocks[0]).toHaveAttribute('tabindex', '0')
  await userEvent.hover(blocks[0])
  expect(await screen.findByRole('tooltip')).toHaveTextContent('00:00 UTC — Unused')
})

it('defaults to today and keeps fixed sample readings when another day is selected', async () => {
  const request = vi.spyOn(providers, 'getDeviceHistory')
  renderWithMantine(<HistoryPage deviceId={history.device_id} />)
  const date = screen.getByLabelText('History date')
  expect(date).toHaveValue(calendarDay())
  await screen.findByRole('table', { name: 'History reports' })
  expect(request).toHaveBeenCalledWith(history.device_id, history.from, history.to)
  fireEvent.change(date, { target: { value: '2027-01-01' } })
  expect(date).toHaveValue('2027-01-01')
  expect(within(screen.getByRole('table', { name: 'History reports' })).getAllByRole('row')).toHaveLength(49)
  expect(screen.getByText(/Sample history from 10 March 2025/)).toBeInTheDocument()
  expect(request).toHaveBeenCalledTimes(1)
})

it('shows Device not found for an unknown identifier without showing another computer history', async () => {
  renderWithMantine(<HistoryPage deviceId="unknown-device" />)
  expect(await screen.findByRole('heading', { name: 'Device not found' })).toBeInTheDocument()
  expect(screen.queryByRole('table')).not.toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Back to computers' })).toHaveAttribute('href', '#computers')
})

it('distinguishes a known computer with no fixture from a missing computer', async () => {
  const device = devices.devices.find(item => item.device_id !== history.device_id)
  renderWithMantine(<HistoryPage deviceId={device.device_id} />)
  expect(await screen.findByText('No sample history available for this computer.')).toBeInTheDocument()
  expect(screen.queryByText('Device not found')).not.toBeInTheDocument()
  expect(screen.queryByRole('table')).not.toBeInTheDocument()
})

it('loads the new device instead of retaining the previous computer reports', async () => {
  const view = renderWithMantine(<HistoryPage deviceId={history.device_id} />)
  await screen.findByRole('table', { name: 'History reports' })
  view.rerender(<MantineProvider theme={theme} defaultColorScheme="light">
    <HistoryPage deviceId="unknown-device" />
  </MantineProvider>)
  expect(await screen.findByRole('heading', { name: 'Device not found' })).toBeInTheDocument()
  expect(screen.queryByRole('table')).not.toBeInTheDocument()
})

it('shows a loading state and lets the admin retry a failed request', async () => {
  vi.spyOn(providers, 'getDeviceHistory').mockRejectedValueOnce(new Error('Fixture failure'))
  renderWithMantine(<HistoryPage deviceId={history.device_id} />)
  expect(screen.getByRole('status', { name: 'Loading computer history' })).toBeInTheDocument()
  expect(await screen.findByRole('alert')).toHaveTextContent('Could not load data. Try again.')
  await userEvent.click(screen.getByRole('button', { name: 'Retry' }))
  expect(await screen.findByRole('table', { name: 'History reports' })).toBeInTheDocument()
})

it('shows zero totals and an empty history state when a known device has no reports', async () => {
  vi.spyOn(providers, 'getDeviceHistory').mockResolvedValueOnce({
    ...history, reports: [], totals: {
      active_time_seconds: 0, idle_time_seconds: 0, unused_time_seconds: 0,
      energy_uj: 0, cost_gbp: 0, co2_kg: 0,
    },
  })
  renderWithMantine(<HistoryPage deviceId={history.device_id} />)
  expect(await screen.findByRole('status', { name: 'History reports' })).toHaveTextContent('No data')
  expect(within(screen.getByRole('region', { name: 'Cost' })).getByText('£0.00')).toBeInTheDocument()
  expect(screen.queryByText('Device not found')).not.toBeInTheDocument()
})

it('renders a populated location from the device provider', async () => {
  const device = devices.devices.find(item => item.device_id === history.device_id)
  vi.spyOn(providers, 'getDevices').mockResolvedValueOnce({ devices: [
    { ...device, location_name: 'Lab 1', building: 'Main building' },
  ] })
  renderWithMantine(<HistoryPage deviceId={history.device_id} />)
  expect(await screen.findByText('Lab 1, Main building')).toBeInTheDocument()
})
