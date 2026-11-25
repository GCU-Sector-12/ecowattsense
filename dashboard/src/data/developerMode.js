// Browser-only developer mode preference used by SettingsPage and HistoryReports.
const STORAGE_KEY = 'ewsDeveloperMode'

/**
 * Reads whether the current browser has developer mode enabled.
 * @returns {boolean} True only when the saved preference is the string "true".
 */
export function readDeveloperMode() {
  return window.localStorage.getItem(STORAGE_KEY) === 'true'
}

/**
 * Saves the developer mode preference in this browser.
 * @param {boolean} enabled - Whether to show raw report fields.
 * @returns {void}
 */
export function saveDeveloperMode(enabled) {
  window.localStorage.setItem(STORAGE_KEY, String(enabled))
}

