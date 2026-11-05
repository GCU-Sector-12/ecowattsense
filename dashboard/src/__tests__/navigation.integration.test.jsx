// INTEGRATION TEST: renders the whole App so the parts work together (menu, pages, theme).
// Proves: the URL hash picks the page, and the theme switch changes the colour scheme.
// Note: if charts crash in jsdom, mock '@mantine/charts' here with vi.mock (not needed so far).
import { act, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import App from '../App.jsx'
import { renderWithMantine } from '../test/render.jsx'

function goToHash(hash) {
  window.location.hash = hash
  act(() => {
    window.dispatchEvent(new HashChangeEvent('hashchange'))
  })
}

it('opens Settings from the hash and switches to the dark theme', async () => {
  renderWithMantine(<App />)
  goToHash('#settings')

  expect(screen.getByRole('heading', { name: 'Settings' })).toBeInTheDocument()
  const menuItem = screen.getByRole('link', { name: 'Settings' })
  expect(menuItem).toHaveAttribute('data-active', 'true')

  await userEvent.click(screen.getByText('Dark'))
  expect(document.documentElement).toHaveAttribute('data-mantine-color-scheme', 'dark')
})

it('shows the dashboard when there is no hash', () => {
  renderWithMantine(<App />)
  expect(screen.getByRole('heading', { name: 'Welcome to EcoWattSense!' })).toBeInTheDocument()
})

// EWS_WEB-23: every sidebar item has its own page, an unknown address shows "Page not found",
// and the right menu item is highlighted.

// The menu items that are highlighted right now (the sidebar marks them with data-active).
function highlightedItems() {
  return document.querySelectorAll('[data-active="true"]')
}

it('opens the Energy Usage page from #energy and highlights Energy Usage', () => {
  renderWithMantine(<App />)
  goToHash('#energy')

  expect(screen.getByRole('heading', { name: 'Energy Usage' })).toBeInTheDocument()
  expect(screen.getByText('Coming in this sprint')).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Energy Usage' })).toHaveAttribute('data-active', 'true')
})

it('opens the history page from #computers/abc and keeps Computers highlighted', () => {
  renderWithMantine(<App />)
  goToHash('#computers/abc')

  expect(screen.getByRole('heading', { name: 'Computer history' })).toBeInTheDocument()
  expect(screen.getByText('Computer abc')).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Computers' })).toHaveAttribute('data-active', 'true')
})

it('opens the Login page from #login and highlights no menu item', () => {
  renderWithMantine(<App />)
  goToHash('#login')

  expect(screen.getByRole('heading', { name: 'Login' })).toBeInTheDocument()
  expect(highlightedItems()).toHaveLength(0)
})

it('shows "Page not found" with a link to the Dashboard for an unknown address', () => {
  renderWithMantine(<App />)
  goToHash('#nonsense')

  expect(screen.getByRole('heading', { name: 'Page not found' })).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Back to the Dashboard' })).toHaveAttribute('href', '#dashboard')
  expect(highlightedItems()).toHaveLength(0)
})

it('has no Reports page: the menu has no Reports item and #reports is "Page not found"', () => {
  renderWithMantine(<App />)
  expect(screen.queryByRole('link', { name: 'Reports' })).not.toBeInTheDocument()

  goToHash('#reports')
  expect(screen.getByRole('heading', { name: 'Page not found' })).toBeInTheDocument()
})

it('shows "Page not found" for #/computers, not the Dashboard', () => {
  renderWithMantine(<App />)
  goToHash('#/computers')

  expect(screen.getByRole('heading', { name: 'Page not found' })).toBeInTheDocument()
  expect(highlightedItems()).toHaveLength(0)
})

it('highlights Dashboard when there is no hash, and Computers on #computers', () => {
  renderWithMantine(<App />)
  expect(screen.getByRole('link', { name: 'Dashboard' })).toHaveAttribute('data-active', 'true')

  goToHash('#computers')
  expect(screen.getByRole('heading', { name: 'Computers' })).toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Computers' })).toHaveAttribute('data-active', 'true')
})
