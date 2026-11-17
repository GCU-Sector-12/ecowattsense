// Ends the demo session when a data call is refused with HTTP 401 (the server forgot the login).
import { clearSampleSession } from './sampleSession.js'

/**
 * Clears the sample session when the response is 401, so the route guard opens Login.
 * Never throws: a failing sessionStorage must not hide the original HTTP error.
 * @param {{status: number}} response - Answer of a data call.
 * @returns {void}
 */
export function endSessionOn401(response) {
  if (response.status !== 401) return
  try {
    clearSampleSession()
  } catch {
    // Storage is unavailable; the caller still throws its own error.
  }
}
