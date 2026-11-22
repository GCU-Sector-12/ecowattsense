// Connect provider: the two admin calls behind the /connect page (contract C2 and C3).
// ConnectPage.jsx uses it to read a connect request by its code and to approve it.
// It always asks the server. There is no sample mode: an approval must be real.
import { endSessionOn401 } from '../auth/unauthorized.js'

const REQUESTS_URL = '/api/admin/connect/requests/'
const FIELDS = ['user_code', 'device_id', 'device_name', 'os', 'device_type', 'approved', 'expires_in_seconds']

/**
 * Builds the address of one connect request. The code is typed by a person, so it is trimmed
 * and encoded: it can never change the path or add a query. The server ignores case and hyphen.
 * @param {string} code - The code as typed or read from the address.
 * @returns {string} The address without a trailing part, for example "/api/admin/connect/requests/ABCD-EFGH".
 */
function requestUrl(code) {
  return REQUESTS_URL + encodeURIComponent(String(code).trim())
}

/**
 * Builds the error for a failed answer. The status stays on the error so the page can tell
 * "not found" (404) from every other failure. A 401 also ends the dashboard session.
 * @param {Response} response - The failed answer.
 * @returns {Error} Error with the HTTP status.
 */
function failure(response) {
  endSessionOn401(response)
  return Object.assign(new Error('Connect request failed (' + response.status + ').'), { status: response.status })
}

/**
 * Rejects an answer that lacks a field of the contract, so it is never shown as a request.
 * @param {*} data - Parsed body of C2.
 * @returns {object} The unchanged body.
 * @throws {Error} When the body is not an object with every field.
 */
function validateRequest(data) {
  const complete = data && typeof data === 'object' && FIELDS.every((name) => name in data)
  if (!complete) throw new Error('Invalid connect response')
  return data
}

/**
 * Reads a connect request: GET /api/admin/connect/requests/{code} (C2).
 * @param {string} code - The code from Setup, for example "ABCD-EFGH".
 * @returns {Promise<object>} user_code, device_id, device_name (or null), os, device_type,
 *   approved and expires_in_seconds.
 * @throws {Error} With a status for an HTTP failure (404 unknown or expired, 401 no session),
 *   without a status for a network failure, or "Invalid connect response" for an incomplete body.
 */
export async function loadConnectRequest(code) {
  const response = await fetch(requestUrl(code), { method: 'GET', credentials: 'include' })
  if (!response.ok) throw failure(response)
  return validateRequest(await response.json())
}

/**
 * Approves a connect request: POST /api/admin/connect/requests/{code}/approve (C3).
 * The server answers 204 with no body, also when the request was approved before.
 * @param {string} code - The code from Setup.
 * @returns {Promise<void>} Resolves when the server has approved the request.
 * @throws {Error} With a status for an HTTP failure (404 unknown or expired, 401 no session),
 *   without a status for a network failure.
 */
export async function approveConnectRequest(code) {
  const response = await fetch(requestUrl(code) + '/approve', { method: 'POST', credentials: 'include' })
  if (!response.ok) throw failure(response)
}
