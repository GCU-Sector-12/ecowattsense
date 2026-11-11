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
  if (!response.ok) throw new Error(result.detail || 'Could not log in. Try again.')
  if (typeof result.username !== 'string' || !result.username) {
    throw new Error('The login response did not include a username.')
  }
  return { username: result.username }
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
