// Integration tests for the EWS_WEB-33 demo login, route guard and logout flow.
import { screen, waitFor } from '@testing-library/react'
import { vi } from 'vitest'
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
  vi.unstubAllGlobals()
  clearSampleSession()
  sessionStorage.clear()
}

/**
 * Checks that the stored sample session has been removed.
 * @returns {void} Completes when the session key is absent.
 */
function expectSessionToBeCleared() {
  expect(sessionStorage.getItem('ews-sample-session')).toBeNull()
}

/**
 * Creates an authenticated browser session for logout integration tests.
 * @returns {void} Stores the admin session and opens the dashboard route.
 */
function setActiveSession() {
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'admin' }))
  window.location.hash = '#dashboard'
}

/**
 * Opens the user menu and selects its logout action.
 * @returns {Promise<void>} Resolves after the logout action is selected.
 */
async function clickLogoutAction() {
  const user = userEvent.setup()
  await user.click(await screen.findByRole('button', { name: 'User menu for admin' }))
  await user.click(await screen.findByRole('menuitem', { name: 'Log out' }))
}

/**
 * Checks that login returns the user to the route they originally requested.
 * @returns {Promise<void>} Resolves when the protected route is restored.
 */
async function redirectsProtectedRouteAfterLogin() {
  const user = userEvent.setup()
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
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

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
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

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
  await user.type(screen.getByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'wrong-password')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(screen.getByRole('alert')).toHaveTextContent('Wrong username or password')
  expect(window.location.hash).toBe('#login')
}

/**
 * Checks that the login card is shown without the dashboard sidebar.
 * @returns {Promise<void>} Resolves when the login card is displayed.
 */
async function showsLoginWithoutSidebar() {
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
  expect(screen.queryByRole('navigation')).not.toBeInTheDocument()
  expect(screen.queryByRole('link', { name: 'Computers' })).not.toBeInTheDocument()
}

/**
 * Checks that logout clears the session and returns the user to login.
 * @returns {Promise<void>} Resolves when the login page is displayed.
 */
async function logsOutAndClearsSession() {
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ status: 204 }))
  setActiveSession()
  renderWithMantine(<App />)

  await clickLogoutAction()

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
  await waitFor(expectSessionToBeCleared)
  expect(window.location.hash).toBe('#login')
  expect(fetch).toHaveBeenCalledWith('/api/logout', { method: 'POST', credentials: 'include' })
}

/**
 * Checks that an HTTP logout failure remains visible and preserves the session.
 * @returns {Promise<void>} Resolves when the failure and retained session are verified.
 */
async function showsHttpLogoutFailureAndKeepsSession() {
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ status: 500 }))
  setActiveSession()
  renderWithMantine(<App />)

  await clickLogoutAction()

  const alert = await screen.findByRole('alert')
  expect(alert).toBeVisible()
  expect(alert).toHaveTextContent('Could not log out. Try again.')
  expect(sessionStorage.getItem('ews-sample-session')).toBe(JSON.stringify({ username: 'admin' }))
  expect(window.location.hash).toBe('#dashboard')
}

/**
 * Checks that a network logout failure remains visible and preserves the session.
 * @returns {Promise<void>} Resolves when the failure and retained session are verified.
 */
async function showsNetworkLogoutFailureAndKeepsSession() {
  vi.stubGlobal('fetch', vi.fn().mockRejectedValue(new Error('Network unavailable')))
  setActiveSession()
  renderWithMantine(<App />)

  await clickLogoutAction()

  const alert = await screen.findByRole('alert')
  expect(alert).toBeVisible()
  expect(alert).toHaveTextContent('Network unavailable')
  expect(sessionStorage.getItem('ews-sample-session')).toBe(JSON.stringify({ username: 'admin' }))
  expect(window.location.hash).toBe('#dashboard')
}

beforeEach(setUpLoginTest)
afterEach(cleanUpLoginTest)

it('redirects a protected route to login and returns there after a successful login', redirectsProtectedRouteAfterLogin)
it('opens the Dashboard after a successful login when no return route is saved', opensDashboardWithoutSavedRoute)
it('shows the required error for incorrect credentials', showsIncorrectCredentialError)
it('shows the login page without the dashboard sidebar', showsLoginWithoutSidebar)
it('clears the session and returns to login when the user logs out', logsOutAndClearsSession)
it('shows an HTTP logout failure and keeps the session active', showsHttpLogoutFailureAndKeepsSession)
it('shows a network logout failure and keeps the session active', showsNetworkLogoutFailureAndKeepsSession)
