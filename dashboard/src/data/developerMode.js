// Browser-only developer mode preference used by SettingsPage and HistoryReports.
const STORAGE_KEY = 'ewsDeveloperMode'

/**
 * Reads whether the current browser has developer mode enabled.
 * Storage may be unavailable in private or restricted browser contexts.
 * @returns {boolean} True only when the saved preference is the string "true".
 */
export function readDeveloperMode() {
  try {
    return window.localStorage.getItem(STORAGE_KEY) === 'true'
  } catch {
    return false
  }
}

/**
 * Saves the developer mode preference in this browser.
 * @param {boolean} enabled - Whether to show raw report fields.
 * @returns {boolean} True when the preference was saved, false when storage is blocked.
 */
export function saveDeveloperMode(enabled) {
  try {
    window.localStorage.setItem(STORAGE_KEY, String(enabled))
    return true
  } catch {
    return false
  }
}
