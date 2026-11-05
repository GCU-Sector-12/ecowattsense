// Local Promise-based sample provider. The API implementation can replace this
// module later without changing the pages that consume these functions.
import summary from './summary.json'
import devices from './devices.json'
import history from './history.json'
import settings from './settings.json'

export async function getSummary(date) {
  return structuredClone(date && date !== summary.date
    ? { ...summary, date }
    : summary)
}

export async function getDevices() {
  return structuredClone(devices)
}

export async function getDeviceHistory(device_id, from, to) {
  const result = structuredClone(history)
  if (device_id && device_id !== result.device_id) return null
  if (from || to) {
    result.reports = result.reports.filter(({ timestamp }) =>
      (!from || timestamp >= from) && (!to || timestamp <= to))
  }
  return result
}

export async function getSettings() {
  return structuredClone(settings)
}
