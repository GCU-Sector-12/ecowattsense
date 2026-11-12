// Sends the dashboard logout request and handles an already-expired session.

/**
 * Ends the admin session through the API.
 * @returns {Promise<void>} Resolves when the session is ended or already invalid.
 * @throws {Error} When the API returns an unexpected status or the request fails.
 */
export async function logout() {
  const response = await fetch('/api/logout', {
    method: 'POST',
    credentials: 'include',
  })

  if (response.status === 204 || response.status === 401) return

  throw new Error('Could not log out. Try again.')
}
