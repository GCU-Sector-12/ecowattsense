// API-unit and display helpers shared by dashboard cards, charts and tables.
// Import these functions to convert microjoules or format currency and last-seen times.
const MICROJOULES_PER_KWH = 3_600_000_000_000

/**
 * Converts API microjoules to kilowatt-hours.
 * @param {number} energy_uj - Energy in microjoules.
 * @returns {number} Energy in kWh.
 */
export function ujToKwh(energy_uj) {
  return energy_uj / MICROJOULES_PER_KWH
}

/**
 * Converts interval energy to average power in kilowatts.
 * @param {number} energy_uj - Energy in microjoules.
 * @param {number} seconds - Finite, positive interval duration.
 * @returns {number} Average power in kW.
 * @throws {RangeError} If the duration is not finite and positive.
 */
export function ujToKw(energy_uj, seconds) {
  if (!Number.isFinite(seconds) || seconds <= 0) {
    throw new RangeError('seconds must be greater than zero')
  }
  return ujToKwh(energy_uj) * 3600 / seconds
}

/**
 * Formats an amount as British pounds using UK currency conventions.
 * @param {number} amount - Amount in pounds.
 * @returns {string} Formatted currency.
 */
export function formatGbp(amount) {
  return new Intl.NumberFormat('en-GB', {
    style: 'currency',
    currency: 'GBP',
  }).format(amount)
}

/**
 * Formats elapsed time; missing timestamps return Never and invalid dates return Unknown.
 * @param {string|Date|null} last_seen_at - Last report timestamp.
 * @param {string|Date} [now=new Date()] - Reference instant.
 * @returns {string} Relative time, treating future timestamps as just now.
 */
export function timeAgo(last_seen_at, now = new Date()) {
  if (!last_seen_at) return 'Never'
  const elapsed = new Date(now).getTime() - new Date(last_seen_at).getTime()
  if (!Number.isFinite(elapsed)) return 'Unknown'
  if (elapsed < 60_000) return 'just now'
  const minutes = Math.floor(elapsed / 60_000)
  if (minutes < 60) return `${minutes} min${minutes === 1 ? '' : 's'} ago`
  const hours = Math.floor(minutes / 60)
  if (hours < 24) return `${hours} hour${hours === 1 ? '' : 's'} ago`
  const days = Math.floor(hours / 24)
  return `${days} day${days === 1 ? '' : 's'} ago`
}
