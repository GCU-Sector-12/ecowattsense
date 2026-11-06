import { beforeEach, expect, it, vi } from 'vitest'
import { screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import ComputersPage from '../pages/ComputersPage.jsx'
import { getDevices } from '../data/index.js'
import devicesFixture from '../data/devices.json'
import { renderWithMantine } from '../test/render.jsx'

vi.mock('../data/index.js', () => ({ getDevices: vi.fn() }))

const noData = {
  ...devicesFixture.devices[0],
  device_id: 'no-data-0001',
  display_name: 'No Report PC',
  state: null,
  state_name: 'no_data',
}

const archived = {
  ...devicesFixture.devices[0],
  device_id: 'archived-0001',
  display_name: 'Archived PC',
  archived: true,
}

beforeEach(() => {
  vi.clearAllMocks()
  getDevices.mockResolvedValue({ devices: [...devicesFixture.devices, archived, noData] })
})

it('searches by the displayed name and sorts by name in both directions', async () => {
  const user = userEvent.setup()
  renderWithMantine(<ComputersPage />)
  expect(await screen.findByRole('cell', { name: 'PC-01' })).toBeInTheDocument()

  await user.click(screen.getByRole('button', { name: 'Sort by Computer Name' }))
  const rows = screen.getAllByRole('row')
  expect(within(rows[1]).getByRole('cell')).toHaveTextContent('PC-20')

  await user.type(screen.getByRole('textbox', { name: 'Search computers' }), 'PC-0')
  expect(screen.getAllByRole('row')).toHaveLength(10)
  expect(screen.queryByRole('cell', { name: 'PC-20' })).not.toBeInTheDocument()
})

it('combines the state filter with name search', async () => {
  const user = userEvent.setup()
  renderWithMantine(<ComputersPage />)
  await screen.findByRole('cell', { name: 'PC-01' })

  await user.click(screen.getByRole('combobox', { name: 'Filter by state' }))
  await user.click(screen.getByRole('option', { name: 'Idle' }))
  expect(screen.getAllByRole('row')).toHaveLength(6)
  expect(screen.getByRole('cell', { name: 'PC-02' })).toBeInTheDocument()
  expect(screen.queryByRole('cell', { name: 'PC-01' })).not.toBeInTheDocument()

  await user.type(screen.getByRole('textbox', { name: 'Search computers' }), 'PC-08')
  expect(screen.getAllByRole('row')).toHaveLength(2)
  expect(screen.getByRole('cell', { name: 'PC-08' })).toBeInTheDocument()
})

it('shows archived computers only after enabling the switch', async () => {
  const user = userEvent.setup()
  renderWithMantine(<ComputersPage />)
  await screen.findByRole('cell', { name: 'PC-01' })
  expect(screen.queryByRole('cell', { name: 'Archived PC' })).not.toBeInTheDocument()

  await user.click(screen.getByRole('checkbox', { name: 'Show archived' }))
  expect(screen.getByRole('cell', { name: 'Archived PC' })).toBeInTheDocument()
  expect(screen.getByText('Showing 22 of 22 monitored computers.')).toBeInTheDocument()
})

it('sorts by status and filters the No data state', async () => {
  const user = userEvent.setup()
  renderWithMantine(<ComputersPage />)
  await screen.findByRole('cell', { name: 'PC-01' })

  await user.click(screen.getByRole('button', { name: 'Sort by Status' }))
  const rows = screen.getAllByRole('row')
  expect(within(rows[1]).getByRole('cell', { name: 'Active' })).toBeInTheDocument()

  await user.click(screen.getByRole('combobox', { name: 'Filter by state' }))
  await user.click(screen.getByRole('option', { name: 'No data' }))
  expect(screen.getByRole('cell', { name: 'No Report PC' })).toBeInTheDocument()
  expect(screen.getAllByRole('row')).toHaveLength(2)
})
