// Settings provider used by getSettings and saveSettings. Select real mode with VITE_SETTINGS_DATA_SOURCE.
// Real failures stay visible to the caller; there is no fallback to sample data.
// Sample mode keeps the settings in memory only, so a save is lost on reload.
import sample from './settings.json'
import { endSessionOn401 } from '../auth/unauthorized.js'

const FIELDS = [
  'settings_version', 'report_interval_seconds', 'ping_interval_seconds', 'idle_threshold_seconds',
  'unused_threshold_seconds', 'no_data_after_cycles', 'tariff_valid_from', 'price_gbp_per_kwh', 'co2_kg_per_kwh',
]
const JSON_HEADERS = { 'Content-Type': 'application/json' }

let sampleState = structuredClone(sample)

/**
 * Tells whether the Settings page reads and saves through the API.
 * @returns {boolean} True when VITE_SETTINGS_DATA_SOURCE is 'real'.
 */
export function isRealSettings() {
  return import.meta.env.VITE_SETTINGS_DATA_SOURCE === 'real'
}

/**
 * Reads the settings: GET /api/admin/settings (contract 5.12) or the in-memory sample.
 * @returns {Promise<object>} The nine settings fields.
 * @throws {Error} For HTTP failures (with status), network failures or an incomplete answer.
 */
export async function loadSettings() {
  if (!isRealSettings()) return structuredClone(sampleState)
  const response = await fetch('/api/admin/settings', { method: 'GET', credentials: 'include' })
  endSessionOn401(response)
  if (!response.ok) throw Object.assign(new Error('Could not load settings (' + response.status + ').'), { status: response.status })
  return validateSettings(await response.json())
}

/**
 * Saves the settings: PUT /api/admin/settings, or a change of the in-memory sample.
 * @param {object} body - All nine fields; settings_version is the one the admin read.
 * @returns {Promise<object>} The saved settings with the new settings_version.
 * @throws {Error} With status and a readable message: 422 carries the server text, 409 means a stale version.
 */
export async function saveSettings(body) {
  if (!isRealSettings()) return saveSample(body)
  const response = await fetch('/api/admin/settings', {
    method: 'PUT', credentials: 'include', headers: JSON_HEADERS, body: JSON.stringify(body),
  })
  endSessionOn401(response)
  if (!response.ok) throw await failure(response)
  return validateSettings(await response.json())
}

/**
 * Stores a sample save in memory and raises the version like the server does.
 * @param {object} body - Submitted settings.
 * @returns {object} A copy of the stored settings.
 */
function saveSample(body) {
  sampleState = { ...body, settings_version: sampleState.settings_version + 1 }
  return structuredClone(sampleState)
}

/**
 * Builds an error with the HTTP status and, when the body has one, the server message.
 * @param {Response} response - A failed answer.
 * @returns {Promise<Error>} Error with status and message.
 */
async function failure(response) {
  const data = await response.json().catch(() => ({}))
  const message = describeDetail(data?.detail) || 'Could not save settings (' + response.status + ').'
  return Object.assign(new Error(message), { status: response.status })
}

/**
 * Turns the detail of an error answer into one line.
 * @param {*} detail - A string, or a list of FastAPI validation items.
 * @returns {string} Readable text, or an empty string when there is none.
 */
function describeDetail(detail) {
  if (typeof detail === 'string') return detail
  if (!Array.isArray(detail)) return ''
  return detail.map(describeItem).join('; ')
}

/**
 * Describes one FastAPI validation item as "field: message".
 * @param {{loc?: Array, msg?: string}} item - One validation item.
 * @returns {string} Readable text.
 */
function describeItem(item) {
  const field = item?.loc?.at(-1)
  return field === undefined ? String(item?.msg ?? '') : field + ': ' + item.msg
}

/**
 * Rejects incomplete answers rather than showing them as settings.
 * @param {*} data - Parsed endpoint response.
 * @returns {object} The unchanged API response.
 * @throws {Error} If a field of the contract is missing.
 */
function validateSettings(data) {
  const complete = data && typeof data === 'object' && FIELDS.every(name => name in data)
  if (!complete) throw new Error('Invalid settings response')
  return data
}
