// Demo-only session helpers shared by the dashboard login, route guard and user menu.
const SESSION_KEY = 'ews-sample-session'
const SESSION_EVENT = 'ews-sample-session-change'

/**
 * Reads the demo session from sessionStorage.
 * @returns {{username: string}|null} The saved username, or null when no session exists.
 */
export function getSampleSession() {
  try {
    const value = JSON.parse(sessionStorage.getItem(SESSION_KEY))
    if (typeof value?.username !== 'string' || !value.username) return null
    return { username: value.username }
  } catch {
    return null
  }
}

/**
 * Stores a demo username and notifies the mounted app.
 * @param {string} username - Username accepted by the sample login form.
 * @returns {{username: string}} The created session.
 */
export function createSampleSession(username) {
  const session = { username }
  sessionStorage.setItem(SESSION_KEY, JSON.stringify(session))
  window.dispatchEvent(new Event(SESSION_EVENT))
  return session
}

/**
 * Clears the demo session and notifies the mounted app.
 * @returns {void}
 */
export function clearSampleSession() {
  sessionStorage.removeItem(SESSION_KEY)
  window.dispatchEvent(new Event(SESSION_EVENT))
}

/**
 * Subscribes to session changes in this browser tab.
 * @param {(session: {username: string}|null) => void} callback - Session state listener.
 * @returns {() => void} Function that removes the listener.
 */
export function subscribeToSession(callback) {
  const update = () => callback(getSampleSession())
  window.addEventListener(SESSION_EVENT, update)
  return () => window.removeEventListener(SESSION_EVENT, update)
}
