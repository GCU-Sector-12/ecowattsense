// Summary provider used by getSummary. Select real mode with VITE_SUMMARY_DATA_SOURCE.
// Keep microjoules and UTC timestamps intact so display components own unit conversion.
import sample from './summary.json'

/**
 * Loads a summary without assuming that every UK day contains 48 buckets.
 * @param {string} [date] - YYYY-MM-DD; omitted means the server's UK today in real mode.
 * @returns {Promise<object>} API-shaped summary; sample mode defaults to its recorded day.
 * @throws {Error} For HTTP failures (with status), network failures or invalid JSON/data.
 */
export async function loadSummary(date) {
  if (import.meta.env.VITE_SUMMARY_DATA_SOURCE !== 'real') return loadSampleSummary(date)
  const query = new URLSearchParams()
  if (date !== undefined) query.set('date', date)
  const suffix = query.size ? '?' + query : ''
  const response = await fetch('/api/summary' + suffix, { method: 'GET', credentials: 'include' })
  if (!response.ok) throw Object.assign(new Error('Could not load summary (' + response.status + ').'), { status: response.status })
  return validateSummary(await response.json())
}

/**
 * Returns one generated daily summary within the 30-day sample window.
 * @param {string} [date=sample.date] - Requested day.
 * @returns {object} Fresh daily response, or empty values outside the sample window.
 */
function loadSampleSummary(date = sample.date) {
  if (date === sample.date) return structuredClone(sample)
  const daysBeforeSample = (Date.parse(sample.date + 'T00:00:00Z') - Date.parse(date + 'T00:00:00Z')) / 86400000
  if (!Number.isInteger(daysBeforeSample) || daysBeforeSample < 0 || daysBeforeSample >= 30) {
    return structuredClone({ ...sample, date, energy_uj: 0, cost_gbp: 0, co2_kg: 0, series: [] })
  }
  const factor = daysBeforeSample === 0 ? 1 : 0.8 + ((daysBeforeSample * 13) % 41) / 100
  return {
    ...structuredClone(sample),
    date,
    energy_uj: Math.round(sample.energy_uj * factor),
    cost_gbp: Number((sample.cost_gbp * factor).toFixed(4)),
    co2_kg: Number((sample.co2_kg * factor).toFixed(4)),
    series: sample.series.map((point) => ({
      ...point,
      timestamp: point.timestamp.replace(sample.date, date),
      energy_uj: Math.round(point.energy_uj * factor),
    })),
  }
}

/**
 * Rejects malformed response containers instead of showing misleading empty data.
 * @param {*} data - Parsed endpoint response.
 * @returns {object} Unchanged API summary.
 * @throws {Error} If counts or the series array is missing.
 */
function validateSummary(data) {
  if (!data || !data.counts || typeof data.counts !== 'object' ||
      Array.isArray(data.counts) || !Array.isArray(data.series)) {
    throw new Error('Invalid summary response')
  }
  return data
}
