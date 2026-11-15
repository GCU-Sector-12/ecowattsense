// Pure rules of the Settings form: form values, validation, change detection and the PUT body.
// Used by SettingsForm.jsx. No React here, so the rules are tested on their own.

const SECONDS_PER_MINUTE = 60

/**
 * Converts the settings of the API to the values the form shows (thresholds in minutes).
 * @param {object} settings - The nine settings fields.
 * @returns {object} Form values.
 */
export function toFormValues(settings) {
  return {
    report_interval_seconds: settings.report_interval_seconds,
    idle_minutes: settings.idle_threshold_seconds / SECONDS_PER_MINUTE,
    unused_minutes: settings.unused_threshold_seconds / SECONDS_PER_MINUTE,
    price_gbp_per_kwh: settings.price_gbp_per_kwh,
    co2_kg_per_kwh: settings.co2_kg_per_kwh,
  }
}

/**
 * Checks the form values with the rules of contract 5.12.
 * @param {object} values - Form values; a field may be '' while the user is typing.
 * @returns {Object<string, string>} Error text by field name; empty when all is valid.
 */
export function validateForm(values) {
  return {
    ...checkInterval(values),
    ...checkThresholds(values),
    ...checkNotNegative(values, 'price_gbp_per_kwh'),
    ...checkNotNegative(values, 'co2_kg_per_kwh'),
  }
}

/**
 * Tells whether a value is a finite number (an empty field is the string '').
 * @param {*} value - Field value.
 * @returns {boolean} True for a finite number.
 */
function isNumber(value) {
  return typeof value === 'number' && Number.isFinite(value)
}

/**
 * Checks the report interval: 60 to 3600 seconds.
 * @param {object} values - Form values.
 * @returns {Object<string, string>} An error or nothing.
 */
function checkInterval(values) {
  const value = values.report_interval_seconds
  const valid = isNumber(value) && Number.isInteger(value) && value >= 60 && value <= 3600
  return valid ? {} : { report_interval_seconds: 'Enter a number from 60 to 3600.' }
}

/**
 * Checks both thresholds: positive, and unused greater than idle.
 * @param {object} values - Form values.
 * @returns {Object<string, string>} Errors or nothing.
 */
function checkThresholds(values) {
  const { idle_minutes: idle, unused_minutes: unused } = values
  const errors = {}
  if (!isNumber(idle) || idle <= 0) errors.idle_minutes = 'Enter minutes greater than 0.'
  if (!isNumber(unused) || unused <= 0) errors.unused_minutes = 'Enter minutes greater than 0.'
  if (Object.keys(errors).length) return errors
  return unused > idle ? {} : { unused_minutes: 'Unused after must be greater than Idle after.' }
}

/**
 * Checks that one field is a number of 0 or more.
 * @param {object} values - Form values.
 * @param {string} name - Field name.
 * @returns {Object<string, string>} An error or nothing.
 */
function checkNotNegative(values, name) {
  const valid = isNumber(values[name]) && values[name] >= 0
  return valid ? {} : { [name]: 'Enter 0 or more.' }
}

/**
 * Tells whether the form differs from the last settings read from the server.
 * @param {object} values - Form values.
 * @param {object} base - Last settings from GET (or from the last save).
 * @returns {boolean} True when at least one field changed.
 */
export function isChanged(values, base) {
  const before = toFormValues(base)
  return Object.keys(before).some(name => values[name] !== before[name])
}

/**
 * Builds the body of PUT /api/admin/settings with all nine fields.
 * Fields without a form input are sent unchanged from the last GET.
 * A changed price or CO2 starts a new tariff period now (UTC, rounded down to the minute).
 * @param {object} values - Valid form values.
 * @param {object} base - Last settings from GET (or from the last save).
 * @param {Date} [now] - Current time.
 * @returns {object} The nine fields.
 */
export function buildPayload(values, base, now = new Date()) {
  const before = toFormValues(base)
  return {
    ...base,
    report_interval_seconds: values.report_interval_seconds,
    idle_threshold_seconds: toSeconds(values.idle_minutes, before.idle_minutes, base.idle_threshold_seconds),
    unused_threshold_seconds: toSeconds(values.unused_minutes, before.unused_minutes, base.unused_threshold_seconds),
    price_gbp_per_kwh: values.price_gbp_per_kwh,
    co2_kg_per_kwh: values.co2_kg_per_kwh,
    tariff_valid_from: isTariffChanged(values, before) ? tariffStart(now) : base.tariff_valid_from,
  }
}

/**
 * Converts minutes back to whole seconds; an untouched value keeps its exact seconds.
 * @param {number} minutes - Value in the form.
 * @param {number} minutesBefore - Value shown after the last GET.
 * @param {number} secondsBefore - Seconds from the last GET.
 * @returns {number} Whole seconds.
 */
function toSeconds(minutes, minutesBefore, secondsBefore) {
  return minutes === minutesBefore ? secondsBefore : Math.round(minutes * SECONDS_PER_MINUTE)
}

/**
 * Tells whether the price or the CO2 factor differs from the last GET.
 * @param {object} values - Form values.
 * @param {object} before - Form values of the last GET.
 * @returns {boolean} True when a new tariff period must start.
 */
function isTariffChanged(values, before) {
  return values.price_gbp_per_kwh !== before.price_gbp_per_kwh || values.co2_kg_per_kwh !== before.co2_kg_per_kwh
}

/**
 * Gives the start of a new tariff period: the time in UTC rounded down to the minute.
 * @param {Date} now - Current time.
 * @returns {string} ISO time like 2026-11-15T10:42:00Z.
 */
function tariffStart(now) {
  const minute = Math.floor(now.getTime() / 60000) * 60000
  return new Date(minute).toISOString().replace('.000Z', 'Z')
}
