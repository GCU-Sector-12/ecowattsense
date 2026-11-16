// INTEGRATION TEST: renders the whole App so the parts work together (menu, pages, theme).
// Proves: the address path picks the page, links and Back work without a reload, an old "#" address
// moves to its plain path, and the theme switch changes the colour scheme.
// Note: if charts crash in jsdom, mock '@mantine/charts' here with vi.mock (not needed so far).
import { act, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import App from '../App.jsx'
import { renderWithMantine } from '../test/render.jsx'
import { createSampleSession, clearSampleSession } from '../auth/sampleSession.js'
import { navigate, redirectOldHash } from '../usePath.js'

beforeEach(() => createSampleSession('admin'))
afterEach(() => clearSampleSession())

/** Opens an address in the mounted app. @param {string} path - Target path, for example "/computers". */
function goToPath(path) {
  act(() => navigate(path))
}

it('opens Settings from the path and switches to the dark theme', async () => {
  renderWithMantine(<App />)
  goToPath('/settings')

  expect(screen.getByRole('heading', { name: 'Settings' })).toBeInTheDocument()
  const menuItem = screen.getByRole('link', { name: 'Settings' })
  expect(menuItem).toHaveAttribute('data-active', 'true')

  expect(await screen.findByLabelText('Report interval (seconds)')).toBeInTheDocument()
  await userEvent.click(screen.getByText('Dark'))
  expect(document.documentElement).toHaveAttribute('data-mantine-color-scheme', 'dark')
})

it('shows the dashboard on the start address', async () => {
  renderWithMantine(<App />)
  expect(await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })).toBeInTheDocument()
  expect(screen.getByLabelText('Active Computers: 60%')).toBeInTheDocument()
  expect(screen.getByLabelText('Idle Computers: 25%')).toBeInTheDocument()
  expect(screen.getByLabelText('Unused Computers: 15%')).toBeInTheDocument()
  expect(screen.getByText('4.8 kWh')).toBeInTheDocument()
  expect(screen.getByText('£1.20')).toBeInTheDocument()
  expect(screen.getByText('0.99 kg')).toBeInTheDocument()
  expect(screen.getAllByRole('row')).toHaveLength(6)
})

/** @returns {NodeListOf<Element>} Currently highlighted navigation items. */
function highlightedItems() {
  return document.querySelectorAll('[data-active="true"]')
}

it('opens the Energy Usage page from /energy and highlights Energy Usage', async () => {
  renderWithMantine(<App />)
  goToPath('/energy')

  expect(screen.getByRole('heading', { name: 'Energy Usage' })).toBeInTheDocument()
  expect(await screen.findByText('Daily breakdown')).toBeInTheDocument()
  expect(screen.getByRole('radio', { name: '7 days' })).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Energy Usage' })).toHaveAttribute('data-active', 'true')
})

it('opens the history page from /computers/abc and keeps Computers highlighted', () => {
  renderWithMantine(<App />)
  goToPath('/computers/abc')

  expect(screen.getByRole('heading', { name: 'Computer history' })).toBeInTheDocument()
  expect(screen.getByText('Computer abc')).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Computers' })).toHaveAttribute('data-active', 'true')
})

it('returns an authenticated user to the dashboard when they open /login', async () => {
  renderWithMantine(<App />)
  goToPath('/login')
  expect(await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })).toBeInTheDocument()
})

it('shows "Page not found" with a link to the Dashboard for an unknown address', () => {
  renderWithMantine(<App />)
  goToPath('/nonsense')

  expect(screen.getByRole('heading', { name: 'Page not found' })).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Back to the Dashboard' })).toHaveAttribute('href', '/dashboard')
  expect(highlightedItems()).toHaveLength(0)
})

it('has no Reports page: the menu has no Reports item and /reports is "Page not found"', () => {
  renderWithMantine(<App />)
  expect(screen.queryByRole('link', { name: 'Reports' })).not.toBeInTheDocument()

  goToPath('/reports')
  expect(screen.getByRole('heading', { name: 'Page not found' })).toBeInTheDocument()
})

it('highlights Dashboard on the start address, and Computers on /computers', async () => {
  renderWithMantine(<App />)
  expect(screen.getByRole('link', { name: 'Dashboard' })).toHaveAttribute('data-active', 'true')

  goToPath('/computers')
  expect(await screen.findByText('Showing 20 of 20 monitored computers.')).toBeInTheDocument()
  expect(screen.getByRole('heading', { name: 'Computers' })).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Computers' })).toHaveAttribute('data-active', 'true')
})

it('opens history from a computer View link', async () => {
  renderWithMantine(<App />)
  goToPath('/computers')
  const link = await screen.findByRole('link', { name: 'View PC-01' })
  const deviceId = link.getAttribute('href').split('/')[2]
  await userEvent.click(link)
  expect(await screen.findByRole('heading', { name: 'Computer history' })).toBeInTheDocument()
  expect(screen.getByText('Computer ' + deviceId)).toBeInTheDocument()
  expect(window.location.pathname).toBe('/computers/' + deviceId)
})

it('opens a page from a menu link without a "#" in the address', async () => {
  renderWithMantine(<App />)
  await userEvent.click(screen.getByRole('link', { name: 'Energy Usage' }))

  expect(screen.getByRole('heading', { name: 'Energy Usage' })).toBeInTheDocument()
  expect(window.location.pathname).toBe('/energy')
  expect(window.location.hash).toBe('')
})

/** @returns {void} Passes when the Computers page is shown. */
function expectComputersPage() {
  expect(screen.getByRole('heading', { name: 'Computers' })).toBeInTheDocument()
}

it('returns to the page before with the Back button of the browser', async () => {
  renderWithMantine(<App />)
  goToPath('/computers')
  goToPath('/energy')
  expect(screen.getByRole('heading', { name: 'Energy Usage' })).toBeInTheDocument()

  window.history.back()
  await waitFor(expectComputersPage)
  expect(window.location.pathname).toBe('/computers')
})

it('opens /computers for an old #computers address', async () => {
  window.history.replaceState(null, '', '/#computers')
  redirectOldHash()
  renderWithMantine(<App />)

  expect(await screen.findByText('Showing 20 of 20 monitored computers.')).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Computers' })).toHaveAttribute('data-active', 'true')
  expect(window.location.pathname).toBe('/computers')
  expect(window.location.hash).toBe('')
})

it.each([
  ['/#computers/abc', '/computers/abc'],
  ['/#/computers', '/computers'],
  ['/#login', '/login'],
  ['/?a=1#energy', '/energy?a=1'],
  ['/computers#top', '/computers#top'],
  ['/settings', '/settings'],
])('redirectOldHash moves only an old hash on the start address: %s gives %s', (before, after) => {
  window.history.replaceState(null, '', before)
  redirectOldHash()
  const { pathname, search, hash } = window.location
  expect(pathname + search + hash).toBe(after)
})
