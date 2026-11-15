// One label rule for a computer, shared by the computer table, the computers page search and sort,
// and the history page title. Import computerLabel for plain text, computerLabelParts for styled text.
const UNNAMED = 'Unnamed computer'

/**
 * Splits a computer label into its readable text and an optional short id.
 * @param {object} computer - Device with display_name (may be null or blank) and device_id.
 * @returns {{text: string, shortId: string|null}} text is the name, or "Unnamed computer" with the
 *   first eight characters of device_id in shortId. A missing device_id gives an empty shortId text.
 */
export function computerLabelParts(computer) {
  const name = typeof computer?.display_name === 'string' ? computer.display_name.trim() : ''
  if (name) return { text: name, shortId: null }
  return { text: UNNAMED, shortId: String(computer?.device_id ?? '').slice(0, 8) }
}

/**
 * Gives one plain-text label for a computer, used for search, sort and aria labels.
 * @param {object} computer - Device with display_name and device_id.
 * @returns {string} The name, or "Unnamed computer (xxxxxxxx)".
 */
export function computerLabel(computer) {
  const { text, shortId } = computerLabelParts(computer)
  return shortId === null ? text : `${text} (${shortId})`
}
