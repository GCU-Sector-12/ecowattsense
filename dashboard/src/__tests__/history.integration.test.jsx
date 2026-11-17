// End-to-end component integration: a computer View link opens its populated sample history.
import { screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import App from '../App.jsx'
import { createSampleSession, clearSampleSession } from '../auth/sampleSession.js'
import { renderWithMantine } from '../test/render.jsx'
import history from '../data/history.json'

/** @returns {void} Starts an authenticated visit to the computers list. */
function startComputerVisit() {
  createSampleSession('admin')
  window.history.replaceState(null, '', '/computers')
}

/** @returns {void} Removes the sample session after the integration case. */
function endComputerVisit() {
  clearSampleSession()
}
beforeEach(startComputerVisit)
afterEach(endComputerVisit)

it('opens totals, the timeline and reports through View PC-01', async () => {
  renderWithMantine(<App />)
  await userEvent.click(await screen.findByRole('link', { name: 'View PC-01' }))
  expect(await screen.findByRole('table', { name: 'History reports' })).toBeInTheDocument()
  expect(window.location.pathname).toBe(`/computers/${history.device_id}`)
  expect(screen.getByRole('heading', { name: 'PC-01' })).toBeInTheDocument()
  expect(within(screen.getByRole('list', { name: 'Daily activity reports' })).getAllByRole('listitem')).toHaveLength(48)
  expect(screen.getByText('18.330 kWh')).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Computers' })).toHaveAttribute('data-active', 'true')
})
