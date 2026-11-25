// SettingsPage with the Settings form: real-mode load and save, answers 422/409/network, validation and sample mode.
import { act, fireEvent, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import SettingsPage from '../pages/SettingsPage.jsx'
import { renderWithMantine } from '../test/render.jsx'
import sample from '../data/settings.json'

const server = { ...sample, settings_version: 7, tariff_valid_from: '2026-09-14T00:00:00Z' }

/** @param {number} status - HTTP status. @param {*} json - Parsed body. @returns {object} Fetch answer. */
function answer(status, json) {
  return { ok: status >= 200 && status < 300, status, json: async () => json }
}

/** @param {object} put - Answer for PUT requests. @returns {Function} Router for fetch. */
function route(put) {
  return async (url, options) => options.method === 'PUT' ? put : answer(200, server)
}

/** @returns {void} Selects real mode and installs fetch that answers a good GET. */
function prepare() {
  vi.stubEnv('VITE_SETTINGS_DATA_SOURCE', 'real')
  vi.stubGlobal('fetch', vi.fn(route(answer(200, { ...server, settings_version: 8 }))))
}

/** @returns {void} Restores globals and modes. */
function restore() {
  vi.unstubAllGlobals()
  vi.unstubAllEnvs()
  vi.useRealTimers()
}
beforeEach(prepare)
afterEach(restore)

/** @returns {object[]} The PUT calls seen by fetch. */
function puts() {
  return fetch.mock.calls.filter(call => call[1].method === 'PUT')
}

/** @param {string} label - Field label. @param {string} value - New text. @returns {void} Types into the field. */
function type(label, value) {
  fireEvent.change(screen.getByLabelText(label), { target: { value } })
}

/** @returns {Promise<void>} Renders the page and waits for the form. */
async function openPage() {
  renderWithMantine(<SettingsPage />)
  await screen.findByLabelText('Report interval (seconds)')
}

/** @returns {Promise<void>} Checks the real values fill the form and the old line is gone. */
async function checkLoad() {
  await openPage()
  expect(screen.getByRole('switch', { name: /Developer mode/ })).not.toBeChecked()
  expect(screen.getByLabelText('Report interval (seconds)')).toHaveValue('300')
  expect(screen.getByLabelText('Idle after (minutes)')).toHaveValue('10')
  expect(screen.getByLabelText('Unused after (minutes)')).toHaveValue('60')
  expect(screen.getByLabelText('Electricity price (GBP per kWh)')).toHaveValue('0.25')
  expect(screen.getByLabelText('CO2 (kg per kWh)')).toHaveValue('0.207')
  expect(screen.queryByText(/Report interval: /)).not.toBeInTheDocument()
  expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled()
}
it('loads the real settings into the form', checkLoad)

/** @returns {Promise<void>} Checks the developer mode choice persists after the Settings page remounts. */
async function checkDeveloperModePersists() {
  const user = userEvent.setup()
  const view = renderWithMantine(<SettingsPage />)
  await screen.findByLabelText('Report interval (seconds)')
  const toggle = screen.getByRole('switch', { name: /Developer mode/ })
  expect(toggle).not.toBeChecked()
  await user.click(toggle)
  expect(toggle).toBeChecked()
  view.unmount()
  renderWithMantine(<SettingsPage />)
  expect(await screen.findByRole('switch', { name: /Developer mode/ })).toBeChecked()
}
it('keeps developer mode enabled after the Settings page reloads', checkDeveloperModePersists)

/** @returns {Promise<void>} Checks a good save: nine fields, tariff time kept, Saved shown for 3 seconds. */
async function checkSave() {
  const user = userEvent.setup()
  await openPage()
  type('Report interval (seconds)', '600')
  await user.click(screen.getByRole('button', { name: 'Save' }))
  expect(await screen.findByText('Saved')).toBeInTheDocument()
  expect(puts()).toHaveLength(1)
  const sent = JSON.parse(puts()[0][1].body)
  expect(Object.keys(sent)).toHaveLength(9)
  expect(sent).toMatchObject({ settings_version: 7, report_interval_seconds: 600, tariff_valid_from: server.tariff_valid_from })
  expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled()
}
it('saves all nine fields and shows Saved', checkSave)

/** @returns {Promise<void>} Checks Saved disappears after 3 seconds. */
async function checkSavedHides() {
  await openPage()
  vi.useFakeTimers({ shouldAdvanceTime: true })
  type('Report interval (seconds)', '600')
  fireEvent.click(screen.getByRole('button', { name: 'Save' }))
  expect(await screen.findByText('Saved')).toBeInTheDocument()
  await act(() => vi.advanceTimersByTimeAsync(3100))
  expect(screen.queryByText('Saved')).not.toBeInTheDocument()
}
it('hides Saved after 3 seconds', checkSavedHides)

/** @returns {Promise<void>} Checks the next save uses the new settings_version from the answer. */
async function checkNewVersion() {
  const user = userEvent.setup()
  await openPage()
  type('Report interval (seconds)', '600')
  await user.click(screen.getByRole('button', { name: 'Save' }))
  await screen.findByText('Saved')
  type('Report interval (seconds)', '900')
  await user.click(screen.getByRole('button', { name: 'Save' }))
  await waitFor(() => expect(puts()).toHaveLength(2))
  expect(JSON.parse(puts()[1][1].body).settings_version).toBe(8)
}
it('uses the returned settings_version for the next save', checkNewVersion)

/** @returns {Promise<void>} Checks a price change starts a new tariff period at the current minute. */
async function checkNewTariff() {
  const user = userEvent.setup()
  await openPage()
  vi.useFakeTimers({ shouldAdvanceTime: true })
  vi.setSystemTime(new Date('2026-11-15T10:42:37Z'))
  type('Electricity price (GBP per kWh)', '0.3')
  await user.click(screen.getByRole('button', { name: 'Save' }))
  await waitFor(() => expect(puts()).toHaveLength(1))
  const sent = JSON.parse(puts()[0][1].body)
  expect(sent).toMatchObject({ price_gbp_per_kwh: 0.3, tariff_valid_from: '2026-11-15T10:42:00Z' })
}
it('sends a new tariff_valid_from when the price changed', checkNewTariff)

/** @returns {Promise<void>} Checks the 422 text is shown and the typed value stays. */
async function check422() {
  const user = userEvent.setup()
  fetch.mockImplementation(route(answer(422, { detail: 'Server says no' })))
  await openPage()
  type('Report interval (seconds)', '600')
  await user.click(screen.getByRole('button', { name: 'Save' }))
  expect(await screen.findByText('Server says no')).toBeInTheDocument()
  expect(screen.getByLabelText('Report interval (seconds)')).toHaveValue('600')
}
it('shows the 422 message and keeps the typed values', check422)

/** @returns {Promise<void>} Checks 409 shows the text and Reload reads GET again. */
async function check409() {
  const user = userEvent.setup()
  fetch.mockImplementation(route(answer(409, { detail: 'Reload' })))
  await openPage()
  type('Report interval (seconds)', '600')
  await user.click(screen.getByRole('button', { name: 'Save' }))
  expect(await screen.findByText('Settings were changed by someone else. Reload to see them.')).toBeInTheDocument()
  fetch.mockImplementation(async () => answer(200, { ...server, settings_version: 9, report_interval_seconds: 1200 }))
  await user.click(screen.getByRole('button', { name: 'Reload' }))
  await waitFor(() => expect(screen.getByLabelText('Report interval (seconds)')).toHaveValue('1200'))
  expect(screen.queryByText(/changed by someone else/)).not.toBeInTheDocument()
}
it('shows the 409 message and reloads on request', check409)

/** @returns {Promise<void>} Checks a network error shows a retry and keeps the typed values. */
async function checkNetwork() {
  const user = userEvent.setup()
  fetch.mockImplementation(async (url, options) => {
    if (options.method === 'PUT') throw new TypeError('network down')
    return answer(200, server)
  })
  await openPage()
  type('Report interval (seconds)', '600')
  await user.click(screen.getByRole('button', { name: 'Save' }))
  expect(await screen.findByText('Could not save the settings. Check the connection and try again.')).toBeInTheDocument()
  expect(screen.getByLabelText('Report interval (seconds)')).toHaveValue('600')
  fetch.mockImplementation(route(answer(200, { ...server, settings_version: 8 })))
  await user.click(screen.getByRole('button', { name: 'Try again' }))
  expect(await screen.findByText('Saved')).toBeInTheDocument()
}
it('shows a retry after a network error and keeps the typed values', checkNetwork)

/** @returns {Promise<void>} Checks a failed first load shows the retry card. */
async function checkLoadFailure() {
  fetch.mockResolvedValue(answer(401, {}))
  renderWithMantine(<SettingsPage />)
  expect(await screen.findByRole('alert')).toHaveTextContent('Could not load data. Try again.')
}
it('shows the error card when the load fails with 401', checkLoadFailure)

/** @param {object} input - Field label, text and the expected error. @returns {Promise<void>} Checks the error and no PUT. */
async function checkValidation({ label, text, message }) {
  const user = userEvent.setup()
  await openPage()
  type(label, text)
  expect(await screen.findByText(message)).toBeInTheDocument()
  expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled()
  await user.click(screen.getByRole('button', { name: 'Save' }))
  expect(puts()).toHaveLength(0)
}
it.each([
  { label: 'Report interval (seconds)', text: '59', message: 'Enter a number from 60 to 3600.' },
  { label: 'Report interval (seconds)', text: '3601', message: 'Enter a number from 60 to 3600.' },
  { label: 'Unused after (minutes)', text: '10', message: 'Unused after must be greater than Idle after.' },
  { label: 'Electricity price (GBP per kWh)', text: '-1', message: 'Enter 0 or more.' },
])('blocks the save when $label is $text', checkValidation)

/** @returns {Promise<void>} Checks sample mode: no fetch, a save stays in the page. */
async function checkSample() {
  vi.stubEnv('VITE_SETTINGS_DATA_SOURCE', '')
  const user = userEvent.setup()
  await openPage()
  type('Report interval (seconds)', '600')
  await user.click(screen.getByRole('button', { name: 'Save' }))
  expect(await screen.findByText('Saved')).toBeInTheDocument()
  expect(fetch).not.toHaveBeenCalled()
}
it('works without the API in sample mode', checkSample)

