// Tests of the connect provider (C2 and C3 of the connect contract). Run with vitest.
// fetch is stubbed, so no real server or account is used.
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { approveConnectRequest, loadConnectRequest } from './connectProvider.js'

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
 * Builds a fake fetch answer.
 * @param {number} status - HTTP status.
 * @param {*} json - Parsed body.
 * @returns {object} Fetch-like answer.
 */
function answer(status, json) {
  return { ok: status >= 200 && status < 300, status, json: async () => json }
}

/** Installs an isolated fetch spy before each test. */
function prepare() {
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue(answer(200, request)))
}

/** Restores globals and clears the session after each test. */
function restore() {
  vi.unstubAllGlobals()
  sessionStorage.clear()
}
beforeEach(prepare)
afterEach(restore)

/** Checks the C2 address, the cookie option and the unchanged body. */
async function checkLoad() {
  expect(await loadConnectRequest('ABCD-EFGH')).toEqual(request)
  expect(fetch).toHaveBeenCalledWith('/api/admin/connect/requests/ABCD-EFGH', {
    method: 'GET', credentials: 'include',
  })
}
it('reads C2 with the session cookie', checkLoad)

/** Checks that spaces are trimmed and unsafe characters are encoded in the address. */
async function checkEncoding() {
  await loadConnectRequest('  ab/cd?x=1 ')
  expect(fetch.mock.calls[0][0]).toBe('/api/admin/connect/requests/ab%2Fcd%3Fx%3D1')
}
it('trims the code and encodes it in the address', checkEncoding)

/** Checks that a lower-case code is sent as typed (the server ignores case). */
async function checkCase() {
  await loadConnectRequest('abcd-efgh')
  expect(fetch.mock.calls[0][0]).toBe('/api/admin/connect/requests/abcd-efgh')
}
it('sends a small-letter code as typed', checkCase)

/** Checks the C3 address, method and cookie option, and the empty answer. */
async function checkApprove() {
  fetch.mockResolvedValue(answer(204, null))
  expect(await approveConnectRequest(' ABCD-EFGH ')).toBeUndefined()
  expect(fetch).toHaveBeenCalledWith('/api/admin/connect/requests/ABCD-EFGH/approve', {
    method: 'POST', credentials: 'include',
  })
}
it('approves with POST C3 and accepts 204', checkApprove)

/**
 * Checks that a failed read keeps the HTTP status.
 * @param {number} status - Failing HTTP status.
 */
async function checkLoadStatus(status) {
  fetch.mockResolvedValue(answer(status, { detail: 'x' }))
  await expect(loadConnectRequest('ABCD-EFGH')).rejects.toMatchObject({ status })
}
it.each([404, 401, 500])('keeps HTTP %s of a read in the error', checkLoadStatus)

/**
 * Checks that a failed approval keeps the HTTP status.
 * @param {number} status - Failing HTTP status.
 */
async function checkApproveStatus(status) {
  fetch.mockResolvedValue(answer(status, { detail: 'x' }))
  await expect(approveConnectRequest('ABCD-EFGH')).rejects.toMatchObject({ status })
}
it.each([404, 401, 500])('keeps HTTP %s of an approval in the error', checkApproveStatus)

/** Checks that a 401 ends the dashboard session, like the other admin calls. */
async function checkSessionEnds() {
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'admin' }))
  fetch.mockResolvedValue(answer(401, { detail: 'Not authenticated' }))
  await expect(loadConnectRequest('ABCD-EFGH')).rejects.toMatchObject({ status: 401 })
  expect(sessionStorage.getItem('ews-sample-session')).toBeNull()
}
it('ends the dashboard session on 401', checkSessionEnds)

/** Checks that a network error reaches the caller without a status. */
async function checkNetwork() {
  fetch.mockRejectedValue(new TypeError('Failed to fetch'))
  await expect(loadConnectRequest('ABCD-EFGH')).rejects.toThrow('Failed to fetch')
  await expect(approveConnectRequest('ABCD-EFGH')).rejects.toThrow('Failed to fetch')
}
it('lets a network error through', checkNetwork)

/** Checks that an answer without the contract fields is an error, not a request. */
async function checkInvalid() {
  fetch.mockResolvedValue(answer(200, { user_code: 'ABCD-EFGH' }))
  await expect(loadConnectRequest('ABCD-EFGH')).rejects.toThrow('Invalid connect response')
}
it('rejects an incomplete read answer', checkInvalid)
