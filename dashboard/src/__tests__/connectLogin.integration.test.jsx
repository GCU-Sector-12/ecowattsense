// INTEGRATION TEST: /connect inside App.jsx. Proves: without a session the Login page opens and
// a good login returns to the same address with its query (/connect?code=...), and the connect
// page is shown without the sidebar.
import { screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import App from '../App.jsx'
import { clearSampleSession } from '../auth/sampleSession.js'
import { renderWithMantine } from '../test/render.jsx'

const request = {
  user_code: 'ABCD-EFGH',
  device_id: '11111111-2222-3333-4444-555555555555',
  device_name: 'PC-01',
  os: 'windows',
  device_type: 'laptop',
  approved: false,
  expires_in_seconds: 412,
}

/**
 * Answers the login call and the connect read like the server does.
 * @param {string} url - Address of the call.
 * @returns {Promise<object>} Fetch-like answer.
 */
function serverAnswer(url) {
  const body = url === '/api/login' ? { username: 'admin' } : request
  return Promise.resolve({ ok: true, status: 200, json: async () => body })
}

/** Starts a test without a session, at the address Setup opens. */
function setUp() {
  vi.stubGlobal('fetch', vi.fn(serverAnswer))
  clearSampleSession()
  sessionStorage.clear()
  window.history.replaceState(null, '', '/connect?code=ABCD-EFGH')
}

/** Clears the session and the stub after a test. */
function cleanUp() {
  vi.unstubAllGlobals()
  clearSampleSession()
  sessionStorage.clear()
}
beforeEach(setUp)
afterEach(cleanUp)

/** Checks the whole way: Login first, then the same address, then the request. */
async function returnsToTheConnectAddressAfterLogin() {
  const user = userEvent.setup()
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
  expect(window.location.pathname).toBe('/login')
  expect(sessionStorage.getItem('ews-return-route')).toBe('/connect?code=ABCD-EFGH')

  await user.type(screen.getByRole('textbox', { name: 'Username' }), 'admin')
  await user.type(screen.getByLabelText('Password'), 'admin12345')
  await user.click(screen.getByRole('button', { name: 'Log in' }))

  expect(await screen.findByRole('heading', { name: 'Connect a computer' })).toBeInTheDocument()
  expect(window.location.pathname + window.location.search).toBe('/connect?code=ABCD-EFGH')
  expect(await screen.findByText('PC-01')).toBeInTheDocument()
  expect(sessionStorage.getItem('ews-return-route')).toBeNull()
}
it('opens Login without a session and returns to /connect?code=... after login', returnsToTheConnectAddressAfterLogin)

/** Checks that a signed-in admin sees the page at once and without the sidebar. */
async function showsThePageWithoutTheSidebar() {
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'admin' }))
  renderWithMantine(<App />)

  expect(await screen.findByRole('heading', { name: 'Connect a computer' })).toBeInTheDocument()
  expect(await screen.findByText('PC-01')).toBeInTheDocument()
  expect(screen.queryByRole('navigation')).not.toBeInTheDocument()
  expect(screen.queryByRole('link', { name: 'Computers' })).not.toBeInTheDocument()
  expect(fetch).toHaveBeenCalledWith('/api/admin/connect/requests/ABCD-EFGH', {
    method: 'GET', credentials: 'include',
  })
}
it('shows /connect without the sidebar for a signed-in admin', showsThePageWithoutTheSidebar)
