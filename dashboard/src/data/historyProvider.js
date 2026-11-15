// History provider used by getDeviceHistory. Select real mode with VITE_HISTORY_DATA_SOURCE.
// Real failures stay visible to the caller; there is no fallback to sample data.

/**
 * Tells whether the history page reads the API instead of the sample file.
 * @returns {boolean} True when VITE_HISTORY_DATA_SOURCE is 'real'.
 */
export function isRealHistory() {
  return import.meta.env.VITE_HISTORY_DATA_SOURCE === 'real'
}

/**
 * Calls the authenticated history endpoint of one computer (contract 5.9).
 * @param {string} deviceId - Computer identifier.
 * @param {string} [from] - Inclusive ISO UTC start; the server picks a default when omitted.
 * @param {string} [to] - Exclusive ISO UTC end; the server picks a default when omitted.
 * @returns {Promise<object|null>} The unchanged API response, or null for an unknown computer (404).
 * @throws {Error} For other HTTP failures (with status), network failures or invalid data.
 */
export async function loadDeviceHistory(deviceId, from, to) {
  const query = new URLSearchParams()
  if (from !== undefined) query.set('from', from)
  if (to !== undefined) query.set('to', to)
  const suffix = query.size ? '?' + query : ''
  const url = '/api/devices/' + encodeURIComponent(deviceId) + '/history' + suffix
  const response = await fetch(url, { method: 'GET', credentials: 'include' })
  if (response.status === 404) return null
  if (!response.ok) throw Object.assign(new Error('Could not load history (' + response.status + ').'), { status: response.status })
  return validateHistory(await response.json())
}

/**
 * Rejects malformed responses rather than showing them as an empty day.
 * @param {*} data - Parsed endpoint response.
 * @returns {object} The unchanged API response.
 * @throws {Error} If reports or totals are missing.
 */
function validateHistory(data) {
  if (!data || !Array.isArray(data.reports) || !data.totals || typeof data.totals !== 'object') {
    throw new Error('Invalid history response')
  }
  return data
}
