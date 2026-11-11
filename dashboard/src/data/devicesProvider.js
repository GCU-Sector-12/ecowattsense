// Device data provider used by getDevices. Select real mode with VITE_DEVICES_DATA_SOURCE.
// Both modes return API-shaped data; real failures remain visible to the caller.
import sample from './devices.json'

/**
 * Loads sample devices or calls the authenticated devices endpoint.
 * Archived rows are included by default because the pages filter them locally.
 * @param {object} [options] - Optional location_id and include_archived filters.
 * @returns {Promise<{devices: object[]}>} A fresh device response.
 * @throws {Error} For HTTP failures (with status), network failures or invalid JSON/data.
 */
export async function loadDevices(options = {}) {
  const { location_id, include_archived = true } = options
  if (import.meta.env.VITE_DEVICES_DATA_SOURCE !== 'real') {
    return filterSampleDevices(location_id, include_archived)
  }
  const query = new URLSearchParams({ include_archived: String(include_archived) })
  if (location_id !== undefined && location_id !== null) query.set('location_id', String(location_id))
  const response = await fetch('/api/devices?' + query, { method: 'GET', credentials: 'include' })
  if (!response.ok) throw Object.assign(new Error('Could not load devices (' + response.status + ').'), { status: response.status })
  return validateDevices(await response.json())
}

/**
 * Filters an independent sample response with the same options as the endpoint.
 * @param {number|undefined} location_id - Optional room.
 * @param {boolean} include_archived - Whether to keep archived rows.
 * @returns {{devices: object[]}} Matching sample rows.
 */
function filterSampleDevices(location_id, include_archived) {
  const result = structuredClone(sample)
  result.devices = result.devices
    .filter(device => location_id == null || device.location_id === location_id)
    .filter(device => include_archived || !device.archived)
  return result
}

/**
 * Rejects malformed responses rather than treating them as an empty success.
 * @param {*} data - Parsed endpoint response.
 * @returns {{devices: object[]}} The unchanged API response.
 * @throws {Error} If the devices array is missing.
 */
function validateDevices(data) {
  if (!data || !Array.isArray(data.devices)) throw new Error('Invalid devices response')
  return data
}
