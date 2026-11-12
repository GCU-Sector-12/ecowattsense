// Real admin authentication requests shared by the dashboard login page.
const LOGIN_URL = '/api/login'

/**
 * Sends admin credentials to the API and returns the authenticated username.
 * @param {string} username - Admin username from the login form.
 * @param {string} password - Admin password from the login form.
 * @returns {Promise<{username: string}>} The username returned by the server.
 * @throws {Error} When the credentials are rejected, the response is invalid, or the request fails.
 */
export async function login(username, password) {
  const response = await fetch(LOGIN_URL, {
    method: 'POST',
    credentials: 'include',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password }),
  })
  const result = await readLoginResponse(response)
  if (!response.ok) throw new Error(getLoginErrorMessage(result.detail))
  if (typeof result.username !== 'string' || !result.username) {
    throw new Error('The login response did not include a username.')
  }
  return { username: result.username }
}

/**
 * Converts API validation details into a message the login form can display.
 * @param {unknown} detail - Error detail returned by the login API.
 * @returns {string} Readable error message or a safe fallback.
 */
function getLoginErrorMessage(detail) {
  if (typeof detail === 'string' && detail) return detail
  if (!Array.isArray(detail)) return 'Could not log in. Try again.'
  const messages = detail.map(formatValidationError).filter(Boolean)
  return messages.join('; ') || 'Could not log in. Try again.'
}

/**
 * Formats one FastAPI validation error without its request-body prefix.
 * @param {unknown} error - One validation error from FastAPI.
 * @returns {string} Readable field and validation message.
 */
function formatValidationError(error) {
  if (!error || typeof error !== 'object') return ''
  const message = typeof error.msg === 'string' ? error.msg : ''
  const location = Array.isArray(error.loc) ? error.loc.filter(isNotBodyLocation).join('.') : ''
  if (!message) return ''
  return location ? `${location}: ${message}` : message
}

/**
 * Checks whether a validation location is more useful than the generic body prefix.
 * @param {unknown} part - One part of a FastAPI validation location.
 * @returns {boolean} True when the location should be shown to the user.
 */
function isNotBodyLocation(part) {
  return part !== 'body'
}

/**
 * Reads the API response body without exposing JSON parsing failures to the caller.
 * @param {Response} response - Login response from the server.
 * @returns {Promise<object>} Parsed response object, or an empty object for an invalid body.
 */
async function readLoginResponse(response) {
  try {
    return await response.json()
  } catch {
    return {}
  }
}
