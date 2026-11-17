// Device data provider used by getDevices and the computer editor.
// Both modes return API-shaped data; sample edits stay in memory for this page session.
import sample from './devices.json'

let sampleDevices = structuredClone(sample.devices)

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
  if (!response.ok) throw httpError('Could not load devices', response.status)
  return validateDevices(await response.json())
}

/**
 * Loads room choices from the authenticated endpoint or current sample devices.
 * @returns {Promise<{locations: object[]}>} Rooms sorted by name and building.
 * @throws {Error} When the real endpoint fails or returns an invalid response.
 */
export async function loadLocations() {
  if (import.meta.env.VITE_DEVICES_DATA_SOURCE !== 'real') return sampleLocations()
  const response = await fetch('/api/locations', { method: 'GET', credentials: 'include' })
  if (!response.ok) throw httpError('Could not load locations', response.status)
  const data = await response.json()
  if (!data || !Array.isArray(data.locations)) throw new Error('Invalid locations response')
  return data
}

/**
 * Saves changed fields for one computer.
 * @param {string} device_id - Computer identifier.
 * @param {object} changes - Changed display_name, location_id and/or archived fields.
 * @returns {Promise<object>} The updated device in device-list response shape.
 * @throws {Error} With status 404, 422 or another HTTP status when the save fails.
 */
export async function updateDevice(device_id, changes) {
  if (import.meta.env.VITE_DEVICES_DATA_SOURCE !== 'real') return updateSampleDevice(device_id, changes)
  const response = await fetch('/api/admin/devices/' + encodeURIComponent(device_id), {
    method: 'PATCH',
    credentials: 'include',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(changes),
  })
  if (!response.ok) throw httpError('Could not save computer', response.status)
  return response.json()
}

/**
 * Filters an independent sample response with the same options as the endpoint.
 * @param {number|undefined} location_id - Optional room.
 * @param {boolean} include_archived - Whether to keep archived rows.
 * @returns {{devices: object[]}} Matching sample rows.
 */
function filterSampleDevices(location_id, include_archived) {
  const devices = structuredClone(sampleDevices)
    .filter(device => location_id == null || device.location_id === location_id)
    .filter(device => include_archived || !device.archived)
  return { devices }
}

/**
 * Builds distinct room choices from the current in-memory sample computers.
 * @returns {{locations: object[]}} Rooms sorted by name.
 */
function sampleLocations() {
  const rooms = new Map()
  sampleDevices.filter(hasRoom).forEach(addSampleRoom.bind(null, rooms))
  return { locations: [...rooms.values()].sort((left, right) => left.name.localeCompare(right.name)) }
}

/**
 * Checks whether a sample computer has a room to offer in the editor.
 * @param {object} device - Sample computer.
 * @returns {boolean} True when both room id and name are present.
 */
function hasRoom(device) {
  return device.location_id !== null && device.location_name !== null
}

/**
 * Adds one sample computer's room to the distinct room map.
 * @param {Map<number, object>} rooms - Room id to room response.
 * @param {object} device - Sample computer with room details.
 * @returns {void}
 */
function addSampleRoom(rooms, device) {
  rooms.set(device.location_id, {
    id: device.location_id, name: device.location_name, building: device.building,
  })
}

/**
 * Applies an edit to the in-memory sample device list.
 * @param {string} device_id - Computer identifier.
 * @param {object} changes - Changed fields.
 * @returns {object} The updated device.
 * @throws {Error} With status 404 when the sample device does not exist.
 */
function updateSampleDevice(device_id, changes) {
  const index = sampleDevices.findIndex(device => device.device_id === device_id)
  if (index < 0) throw httpError('Computer not found', 404)
  sampleDevices[index] = { ...sampleDevices[index], ...changes }
  return structuredClone(sampleDevices[index])
}

/**
 * Creates an HTTP error with a status code for the page and tests.
 * @param {string} message - Human-readable failure.
 * @param {number} status - HTTP response status.
 * @returns {Error} Error carrying the status.
 */
function httpError(message, status) {
  return Object.assign(new Error(message + ' (' + status + ').'), { status })
}

/**
 * Rejects malformed device responses rather than treating them as an empty success.
 * @param {*} data - Parsed endpoint response.
 * @returns {{devices: object[]}} The unchanged API response.
 * @throws {Error} If the devices array is missing.
 */
function validateDevices(data) {
  if (!data || !Array.isArray(data.devices)) throw new Error('Invalid devices response')
  return data
}
