// Integration tests for the EWS_WEB-33 demo login, route guard and logout flow.
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import App from '../App.jsx'
import { clearSampleSession } from '../auth/sampleSession.js'
import { renderWithMantine } from '../test/render.jsx'

beforeEach(() => {
  clearSampleSession()
  sessionStorage.clear()
  window.location.hash = '#computers'
})

afterEach(() => {
  clearSampleSession()
  sessionStorage.clear()
})

it('redirects a protected route to login and returns there after a successful login', async () => {
  const user = userEvent.setup()
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'EcoWattSense' })).toBeInTheDocument()
  expect(window.location.hash).toBe('#login')

  await user.type(screen.getByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(await screen.findByRole('heading', { name: 'Computers' })).toBeInTheDocument()
  expect(screen.getByText('admin')).toBeInTheDocument()
})

it('shows the required error for incorrect credentials', async () => {
  const user = userEvent.setup()
  renderWithMantine(<App />)

  await user.type(await screen.findByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'wrong-password')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(screen.getByRole('alert')).toHaveTextContent('Wrong username or password')
  expect(window.location.hash).toBe('#login')
})

it('clears the session and returns to login when the user logs out', async () => {
  const user = userEvent.setup()
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'admin' }))
  window.location.hash = '#dashboard'
  renderWithMantine(<App />)

  await user.click(await screen.findByRole('button', { name: 'User menu for admin' }))
  await user.click(await screen.findByRole('menuitem', { name: 'Log out' }))

  expect(await screen.findByRole('heading', { name: 'EcoWattSense' })).toBeInTheDocument()
  await waitFor(() => expect(sessionStorage.getItem('ews-sample-session')).toBeNull())
  expect(window.location.hash).toBe('#login')
})
