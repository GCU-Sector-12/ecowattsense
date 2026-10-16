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
