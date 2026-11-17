// Integration test: a 401 from a data call ends the session and sends the user to Login.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { screen, waitFor } from '@testing-library/react'
import App from '../App.jsx'
import { renderWithMantine } from '../test/render.jsx'

vi.mock('../data/useLatestRelease.js', () => ({
  useLatestRelease: () => ({ version: null, url: '#', localVersion: null }),
  installState: () => 'install',
}))

/**
 * Starts a logged-in session on a page with real data and an API that answers 401.
 * @param {string} path - Page to open.
 * @returns {void}
 */
function openExpired(path) {
  vi.stubEnv('VITE_SUMMARY_DATA_SOURCE', 'real')
  vi.stubEnv('VITE_DEVICES_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: false, status: 401 }))
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'admin' }))
  window.history.replaceState(null, '', path)
}

beforeEach(() => sessionStorage.clear())

/** Restores globals and storage. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
  sessionStorage.clear()
}
afterEach(restore)

/**
 * Opens a page whose data call gets 401 and checks the move to Login.
 * @param {string} path - Page path.
 * @returns {Promise<void>}
 */
async function expectLoginFrom(path) {
  openExpired(path)
  renderWithMantine(<App />)
  await waitFor(() => expect(window.location.pathname).toBe('/login'))
  expect(sessionStorage.getItem('ews-return-route')).toBe(path)
  expect(sessionStorage.getItem('ews-sample-session')).toBeNull()
  expect(await screen.findByRole('heading', { name: 'Welcome back' })).toBeInTheDocument()
}

it('goes to Login when the summary call gets 401', () => expectLoginFrom('/dashboard'))
it('goes to Login when the devices call gets 401', () => expectLoginFrom('/computers'))
