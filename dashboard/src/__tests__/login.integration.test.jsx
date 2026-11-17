// Integration tests for dashboard API login, route guarding and logout.
import { StrictMode } from 'react'
import { screen, waitFor } from '@testing-library/react'
import { vi } from 'vitest'
import userEvent from '@testing-library/user-event'
import App from '../App.jsx'
import { clearSampleSession } from '../auth/sampleSession.js'
import { renderWithMantine } from '../test/render.jsx'

// The sidebar reads the release manifest through fetch; this file counts fetch calls, so the hook is replaced.
vi.mock('../data/useLatestRelease.js', () => ({
  useLatestRelease: () => ({ version: null, url: '#', localVersion: null }),
  installState: () => 'install',
}))

/** Resets browser state before each login integration test. */
function setUpLoginTest() {
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
    ok: true,
    json: async () => ({ username: 'admin' }),
  }))
  clearSampleSession()
  sessionStorage.clear()
  window.history.replaceState(null, '', '/computers')
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
  window.history.replaceState(null, '', '/dashboard')
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
  expect(window.location.pathname).toBe('/login')

  await user.type(screen.getByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(fetch).toHaveBeenCalledWith('/api/login', {
    method: 'POST',
    credentials: 'include',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username: 'admin', password: 'admin12345' }),
  })
  expect(await screen.findByRole('heading', { name: 'Computers' })).toBeInTheDocument()
  expect(screen.getByText('admin')).toBeInTheDocument()
  expect(window.location.pathname).toBe('/computers')
}

/**
 * Types the admin name and password into the login form and sends it.
 * @param {ReturnType<typeof userEvent.setup>} user - The user-event session of the test.
 * @returns {Promise<void>} Resolves after the Log in button is clicked.
 */
async function submitLogin(user) {
  await user.type(await screen.findByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  await user.click(screen.getByRole('button', { name: 'Log in' }))
}

/**
 * Checks the return path under React StrictMode, which runs the route guard twice as the dev
 * server does (EWS_WEB-51): the second run must not save /login as the path to return to.
 * @returns {Promise<void>} Resolves when the protected page is restored after login.
 */
async function returnsToProtectedRouteUnderStrictMode() {
  const user = userEvent.setup()
  renderWithMantine(<StrictMode><App /></StrictMode>)

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
  expect(window.location.pathname).toBe('/login')
  expect(sessionStorage.getItem('ews-return-route')).toBe('/computers')

  await submitLogin(user)

  expect(await screen.findByRole('heading', { name: 'Computers' })).toBeInTheDocument()
  expect(window.location.pathname).toBe('/computers')
  expect(sessionStorage.getItem('ews-return-route')).toBeNull()
}

/**
 * Checks that a return route saved as an old hash address, before EWS_WEB-57, opens the Dashboard.
 * @returns {Promise<void>} Resolves when Dashboard is displayed at /dashboard.
 */
async function opensDashboardForOldSavedHashRoute() {
  const user = userEvent.setup()
  sessionStorage.setItem('ews-return-route', '#computers')
  window.history.replaceState(null, '', '/login')
  renderWithMantine(<App />)

  await submitLogin(user)

  expect(await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })).toBeInTheDocument()
  expect(window.location.pathname).toBe('/dashboard')
}

/**
 * Checks that the start address "/" is saved as /dashboard and opens the Dashboard after login.
 * @returns {Promise<void>} Resolves when Dashboard is displayed at /dashboard.
 */
async function returnsToDashboardFromStartAddress() {
  const user = userEvent.setup()
  window.history.replaceState(null, '', '/')
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
  expect(sessionStorage.getItem('ews-return-route')).toBe('/dashboard')
  await submitLogin(user)

  expect(await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })).toBeInTheDocument()
  expect(window.location.pathname).toBe('/dashboard')
}

/**
 * Checks that login opens Dashboard when no return route is saved.
 * @returns {Promise<void>} Resolves when Dashboard is displayed.
 */
async function opensDashboardWithoutSavedRoute() {
  const user = userEvent.setup()
  window.history.replaceState(null, '', '/login')
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
  expect(sessionStorage.getItem('ews-return-route')).toBeNull()

  await user.type(screen.getByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })).toBeInTheDocument()
  expect(window.location.pathname).toBe('/dashboard')
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
  vi.mocked(fetch).mockResolvedValueOnce({
    ok: false,
    json: async () => ({ detail: 'Wrong username or password' }),
  })
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(screen.getByRole('alert')).toHaveTextContent('Wrong username or password')
  expect(window.location.pathname).toBe('/login')
}

/**
 * Checks that FastAPI validation arrays become readable login feedback.
 * @returns {Promise<void>} Resolves when the validation message is displayed.
 */
async function showsReadableValidationError() {
  const user = userEvent.setup()
  renderWithMantine(<App />)

  await user.type(await screen.findByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  vi.mocked(fetch).mockResolvedValueOnce({
    ok: false,
    status: 422,
    json: async () => ({
      detail: [
        { loc: ['body', 'username'], msg: 'Field required', type: 'missing' },
        { loc: ['body', 'password'], msg: 'Field required', type: 'missing' },
      ],
    }),
  })
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(await screen.findByRole('alert')).toHaveTextContent('username: Field required; password: Field required')
  expect(screen.getByRole('alert')).not.toHaveTextContent('[object Object]')
  expect(window.location.pathname).toBe('/login')
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
  expect(window.location.pathname).toBe('/login')
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
  expect(window.location.pathname).toBe('/dashboard')
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
  expect(window.location.pathname).toBe('/dashboard')
}


/**
 * Checks the API login and logout requests together without a pre-seeded session.
 * @returns {Promise<void>} Resolves after logout clears the newly authenticated session.
 */
async function logsInAndOutThroughApi() {
  const user = userEvent.setup()
  vi.mocked(fetch)
    .mockResolvedValueOnce({ ok: true, json: async () => ({ username: 'admin' }) })
    .mockResolvedValueOnce({ status: 204 })
  renderWithMantine(<App />)

  await user.type(await screen.findByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  await user.click(screen.getByRole('button', { name: 'Log in' }))
  expect(await screen.findByRole('heading', { name: 'Computers' })).toBeInTheDocument()
  await clickLogoutAction()

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
  await waitFor(expectSessionToBeCleared)
  expect(window.location.pathname).toBe('/login')
  expect(fetch).toHaveBeenCalledTimes(2)
  expect(fetch).toHaveBeenNthCalledWith(1, '/api/login', {
    method: 'POST', credentials: 'include',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username: 'admin', password: 'admin12345' }),
  })
  expect(fetch).toHaveBeenNthCalledWith(2, '/api/logout', {
    method: 'POST', credentials: 'include',
  })
}

beforeEach(setUpLoginTest)
afterEach(cleanUpLoginTest)

it('redirects a protected route to login and returns there after a successful login', redirectsProtectedRouteAfterLogin)
it('opens the Dashboard after a successful login when no return route is saved', opensDashboardWithoutSavedRoute)
it('returns to the protected route when StrictMode runs the route guard twice', returnsToProtectedRouteUnderStrictMode)
it('opens the Dashboard when the saved return route is an old hash address', opensDashboardForOldSavedHashRoute)
it('saves the start address as /dashboard and opens the Dashboard after login', returnsToDashboardFromStartAddress)
it('shows the required error for incorrect credentials', showsIncorrectCredentialError)
it('shows readable validation errors for a 422 response', showsReadableValidationError)
it('shows the login page without the dashboard sidebar', showsLoginWithoutSidebar)
it('clears the session and returns to login when the user logs out', logsOutAndClearsSession)
it('shows an HTTP logout failure and keeps the session active', showsHttpLogoutFailureAndKeepsSession)
it('shows a network logout failure and keeps the session active', showsNetworkLogoutFailureAndKeepsSession)
it('logs in and out through both API endpoints', logsInAndOutThroughApi)
