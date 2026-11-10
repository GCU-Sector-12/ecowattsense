// Integration tests for the EWS_WEB-33 demo login, route guard and logout flow.
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import App from '../App.jsx'
import { clearSampleSession } from '../auth/sampleSession.js'
import { renderWithMantine } from '../test/render.jsx'

/** Resets browser state before each login integration test. */
function setUpLoginTest() {
  clearSampleSession()
  sessionStorage.clear()
  window.location.hash = '#computers'
}

/** Clears browser state after each login integration test. */
function cleanUpLoginTest() {
  clearSampleSession()
  sessionStorage.clear()
}

/**
 * Checks that login returns the user to the route they originally requested.
 * @returns {Promise<void>} Resolves when the protected route is restored.
 */
async function redirectsProtectedRouteAfterLogin() {
  const user = userEvent.setup()
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'EcoWattSense' })).toBeInTheDocument()
  expect(window.location.hash).toBe('#login')

  await user.type(screen.getByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(await screen.findByRole('heading', { name: 'Computers' })).toBeInTheDocument()
  expect(screen.getByText('admin')).toBeInTheDocument()
}

/**
 * Checks that login opens Dashboard when no return route is saved.
 * @returns {Promise<void>} Resolves when Dashboard is displayed.
 */
async function opensDashboardWithoutSavedRoute() {
  const user = userEvent.setup()
  window.location.hash = '#login'
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'EcoWattSense' })).toBeInTheDocument()
  expect(sessionStorage.getItem('ews-return-route')).toBeNull()

  await user.type(screen.getByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })).toBeInTheDocument()
  expect(window.location.hash).toBe('#dashboard')
}

/**
 * Checks that incorrect credentials show the required error message.
 * @returns {Promise<void>} Resolves when the error is displayed.
 */
async function showsIncorrectCredentialError() {
  const user = userEvent.setup()
  renderWithMantine(<App />)

  await user.type(await screen.findByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'wrong-password')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(screen.getByRole('alert')).toHaveTextContent('Wrong username or password')
  expect(window.location.hash).toBe('#login')
}

/**
 * Checks that logout clears the session and returns the user to login.
 * @returns {Promise<void>} Resolves when the login page is displayed.
 */
async function logsOutAndClearsSession() {
  const user = userEvent.setup()
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'admin' }))
  window.location.hash = '#dashboard'
  renderWithMantine(<App />)

  await user.click(await screen.findByRole('button', { name: 'User menu for admin' }))
  await user.click(await screen.findByRole('menuitem', { name: 'Log out' }))

  expect(await screen.findByRole('heading', { name: 'EcoWattSense' })).toBeInTheDocument()
  /** Checks that logout removed the stored session. */
  function expectSessionToBeCleared() {
    expect(sessionStorage.getItem('ews-sample-session')).toBeNull()
  }
  await waitFor(expectSessionToBeCleared)
  expect(window.location.hash).toBe('#login')
}

beforeEach(setUpLoginTest)
afterEach(cleanUpLoginTest)

it('redirects a protected route to login and returns there after a successful login', redirectsProtectedRouteAfterLogin)
it('opens the Dashboard after a successful login when no return route is saved', opensDashboardWithoutSavedRoute)
it('shows the required error for incorrect credentials', showsIncorrectCredentialError)
it('clears the session and returns to login when the user logs out', logsOutAndClearsSession)
