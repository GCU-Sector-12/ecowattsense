// Unit tests for the dashboard logout API request.
import { afterEach, describe, expect, it, vi } from 'vitest'
import { logout } from '../auth/logout.js'

/** Restores global fetch after each logout API test. */
function restoreFetch() {
  vi.unstubAllGlobals()
}

/**
 * Checks the request method, cookie credentials and absence of a request body.
 * @returns {void} Completes when the request options match the logout contract.
 */
function expectLogoutRequest() {
  expect(fetch).toHaveBeenCalledWith('/api/logout', {
    method: 'POST',
    credentials: 'include',
  })
}

afterEach(restoreFetch)

describe('logout', () => {
  it('sends the logout request and accepts a 204 response', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ status: 204 }))

    await expect(logout()).resolves.toBeUndefined()
    expectLogoutRequest()
  })

  it('accepts a 401 response when the session is already invalid', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ status: 401 }))

    await expect(logout()).resolves.toBeUndefined()
    expectLogoutRequest()
  })

  it('rejects unexpected API response statuses', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ status: 500 }))

    await expect(logout()).rejects.toThrow('Could not log out. Try again.')
    expectLogoutRequest()
  })

  it('propagates network failures from the logout request', async () => {
    vi.stubGlobal('fetch', vi.fn().mockRejectedValue(new Error('Network unavailable')))

    await expect(logout()).rejects.toThrow('Network unavailable')
    expectLogoutRequest()
  })
})
