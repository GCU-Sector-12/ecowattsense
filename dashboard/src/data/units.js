// Energy values in the API are microjoules (µJ), not kWh or watts.
const MICROJOULES_PER_KWH = 3_600_000_000_000

export function ujToKwh(energy_uj) {
  return energy_uj / MICROJOULES_PER_KWH
}

export function ujToKw(energy_uj, seconds) {
  if (!Number.isFinite(seconds) || seconds <= 0) {
    throw new RangeError('seconds must be greater than zero')
  }
  return ujToKwh(energy_uj) * 3600 / seconds
}

export function formatGbp(amount) {
  return new Intl.NumberFormat('en-GB', {
    style: 'currency',
    currency: 'GBP',
  }).format(amount)
}

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
