import { screen, within } from '@testing-library/react'
import devices from '../data/devices.json'
import ComputerTable from '../components/ComputerTable.jsx'
import { renderWithMantine } from '../test/render.jsx'

const computers = devices.devices

it('shows one row per computer plus the header', () => {
  renderWithMantine(<ComputerTable computers={computers} />)
  expect(screen.getAllByRole('row')).toHaveLength(computers.length + 1)
})

it('shows each status as a word, not only a colour', () => {
  renderWithMantine(<ComputerTable computers={computers} />)
  expect(screen.getAllByText('Active').length).toBeGreaterThan(0)
  expect(screen.getAllByText('Idle').length).toBeGreaterThan(0)
  expect(screen.getAllByText('Unused').length).toBeGreaterThan(0)
})

it('uses the default title', () => {
  renderWithMantine(<ComputerTable computers={computers} />)
  expect(screen.getByText('Computer List')).toBeInTheDocument()
})

it('shows a custom title', () => {
  renderWithMantine(<ComputerTable computers={computers} title="Lab 3" />)
  expect(screen.getByText('Lab 3')).toBeInTheDocument()
  expect(screen.queryByText('Computer List')).not.toBeInTheDocument()
})

function device(overrides = {}) {
  return {
    device_id: '12345678-1234-4234-8234-123456789abc',
    display_name: 'Lab PC', state: 2, state_name: 'active',
    last_seen_at: '2025-03-10T14:30:00Z', location_name: null, building: null,
    os: 'windows', device_type: 'laptop', archived: false, ...overrides,
  }
}

it('uses the first eight ID characters when a device has no display name', () => {
  renderWithMantine(<ComputerTable computers={[device({ display_name: null })]} />)
  expect(screen.getByRole('cell', { name: '12345678' })).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'View 12345678' }))
    .toHaveAttribute('href', '#computers/12345678-1234-4234-8234-123456789abc')
})

it('shows all four state words, including No data', () => {
  const states = ['active', 'idle', 'unused', 'no_data']
  renderWithMantine(<ComputerTable computers={states.map((state, index) =>
    device({ device_id: String(index), state_name: state, state: state === 'no_data' ? null : 2 - index }))} />)
  for (const name of ['Active', 'Idle', 'Unused', 'No data']) {
    expect(screen.getByText(name)).toBeInTheDocument()
  }
})

it('shows relative last-seen time with the full timestamp, location, OS and type', () => {
  renderWithMantine(<ComputerTable
    computers={[device({ location_name: 'Lab 1', building: 'George Moore' })]}
    referenceTime="2025-03-10T14:32:00Z" />)
  expect(screen.getByText('2 mins ago')).toHaveAttribute('title', '2025-03-10T14:30:00Z')
  expect(screen.getByRole('cell', { name: 'Lab 1, George Moore' })).toBeInTheDocument()
  expect(screen.getByRole('cell', { name: 'windows / laptop' })).toBeInTheDocument()
})

it.each([
  [{}, 'not set'],
  [{ location_name: 'Lab 1' }, 'Lab 1'],
  [{ building: 'George Moore' }, 'George Moore'],
])('handles partial location fields', (location, expected) => {
  renderWithMantine(<ComputerTable computers={[device(location)]} />)
  expect(screen.getByRole('cell', { name: expected })).toBeInTheDocument()
})

it('hides archived devices by default', () => {
  renderWithMantine(<ComputerTable computers={[
    device(), device({ device_id: 'archived', display_name: 'Old PC', archived: true }),
  ]} />)
  expect(screen.getAllByRole('row')).toHaveLength(2)
  expect(screen.queryByRole('cell', { name: 'Old PC' })).not.toBeInTheDocument()
})

it('supports the showArchived input for the future page switch', () => {
  renderWithMantine(<ComputerTable showArchived computers={[
    device(), device({ device_id: 'archived', display_name: 'Old PC', archived: true }),
  ]} />)
  expect(screen.getByRole('cell', { name: 'Old PC' })).toBeInTheDocument()
})

it('takes the first five non-archived devices and links to the full list', () => {
  const rows = [
    device({ device_id: 'archived', display_name: 'Old PC', archived: true }),
    ...Array.from({ length: 6 }, (_, index) =>
      device({ device_id: String(index), display_name: 'PC ' + index })),
  ]
  renderWithMantine(<ComputerTable computers={rows} limit={5} showAllLink />)
  expect(screen.getAllByRole('row')).toHaveLength(6)
  expect(screen.queryByRole('cell', { name: 'PC 5' })).not.toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'All computers' })).toHaveAttribute('href', '#computers')
  const first = screen.getAllByRole('row')[1]
  expect(within(first).getByRole('link', { name: 'View PC 0' })).toHaveAttribute('href', '#computers/0')
})
