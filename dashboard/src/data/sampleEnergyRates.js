// Sample-only device-type watts for the Settings page. The admin API has no energy-rate endpoint.
const DEFAULT_RATES = [
  { device_type: 'desktop', active_watts: 55, idle_watts: 40, unused_watts: 25 },
  { device_type: 'laptop', active_watts: 15, idle_watts: 9, unused_watts: 3 },
]
export const WATT_FIELDS = ['active_watts', 'idle_watts', 'unused_watts']
let sampleRates = structuredClone(DEFAULT_RATES)

/** @returns {object[]} Independent sample rows using the defaults from the server seed. */
export function readSampleRates() {
  return structuredClone(sampleRates)
}

/**
 * Checks every watt input; blank, negative and nonfinite values are invalid.
 * @param {object[]} rates - Candidate sample rows.
 * @returns {object} Field errors keyed by row and watt name.
 */
export function validateRates(rates) {
  return Object.assign({}, ...rates.flatMap((rate, index) => WATT_FIELDS.map(field => rateError(rate[field], index, field))))
}

/** @param {*} value - Watt input. @param {number} index - Row. @param {string} field - Watt name. @returns {object} Error or an empty object. */
function rateError(value, index, field) {
  const valid = typeof value === 'number' && Number.isFinite(value) && value >= 0
  return valid ? {} : { [`${index}.${field}`]: 'Enter watts of 0 or more.' }
}

/**
 * Atomically stores valid sample rates in this tab's memory without an API call.
 * @param {object[]} rates - Complete candidate rows.
 * @returns {object[]} Independent saved rows.
 * @throws {Error} With field errors when a watt value is invalid; nothing is saved.
 */
export function saveSampleRates(rates) {
  const errors = validateRates(rates)
  if (Object.keys(errors).length) throw Object.assign(new Error('Check the watt fields.'), { errors })
  sampleRates = structuredClone(rates)
  return readSampleRates()
}

/** @returns {void} Restores defaults for isolated tests; a page reload recreates the same defaults. */
export function resetSampleRates() {
  sampleRates = structuredClone(DEFAULT_RATES)
}
