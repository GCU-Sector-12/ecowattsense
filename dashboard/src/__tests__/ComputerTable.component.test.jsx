// COMPONENT TEST: renders one React component alone and checks what the user sees.
// Proves: ComputerTable shows every computer, shows status as a word, and accepts a title.
import { screen } from '@testing-library/react'
import data from '../../../docs/design/dashboard-sample-data.json'
import ComputerTable from '../components/ComputerTable.jsx'
import { renderWithMantine } from '../test/render.jsx'

const computers = data.computers

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
