// EWS_WEB-32 additions on top of EWS_WEB-55: real reporting saves, sample rates/date, and Dashboard captions.
import { act, fireEvent, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import App from '../App.jsx'
import SettingsPage from '../pages/SettingsPage.jsx'
import EnergyRatesSettings from '../components/EnergyRatesSettings.jsx'
import { renderWithMantine } from '../test/render.jsx'
import { createSampleSession, clearSampleSession } from '../auth/sampleSession.js'
import { getSettings, saveSettings } from '../data/index.js'
import { readSampleRates, resetSampleRates } from '../data/sampleEnergyRates.js'
import sample from '../data/settings.json'

/** @returns {Promise<void>} Starts isolated sample state; each real case explicitly selects the API. */
async function prepare() {
  vi.stubEnv('VITE_SETTINGS_DATA_SOURCE', '')
  await saveSettings(sample)
  resetSampleRates()
  createSampleSession('admin')
  window.history.replaceState(null, '', '/settings')
}
/** @returns {void} Restores request globals, modes and sample rate state. */
function restore() {
  vi.unstubAllEnvs()
  vi.unstubAllGlobals()
  act(() => clearSampleSession())
  resetSampleRates()
}
beforeEach(prepare)
afterEach(restore)

/** @param {string} label - Accessible input name. @param {string} value - New text. @returns {void} Edits a field. */
function edit(label, value) {
  fireEvent.change(screen.getByLabelText(label), { target: { value } })
}

/** @returns {void} Installs a versioned settings API without changing the sample device or summary providers. */
function useRealApi() {
  vi.stubEnv('VITE_SETTINGS_DATA_SOURCE', 'real')
  let stored = { ...sample, settings_version: 7 }
  /** @param {string} url - Endpoint URL. @param {object} options - Fetch options. @returns {Promise<object>} API-shaped answer. */
  async function request(url, options) {
    if (options?.method === 'PUT') stored = { ...JSON.parse(options.body), settings_version: stored.settings_version + 1 }
    return { ok: true, status: 200, json: async () => stored }
  }
  vi.stubGlobal('fetch', vi.fn(request))
}

it('saves real ping/cycles/thresholds with credentials and updates Dashboard captions', async () => {
  useRealApi()
  const user = userEvent.setup()
  renderWithMantine(<App />)
  await screen.findByLabelText('Ping interval (seconds)')
  edit('Ping interval (seconds)', '180')
  edit('Missed report cycles before no data', '5')
  edit('Idle after (minutes)', '15')
  edit('Unused after (minutes)', '120')
  await user.click(screen.getByRole('button', { name: 'Save', exact: true }))
  await screen.findByText('Saved', { exact: true })
  const put = fetch.mock.calls.find(call => call[1]?.method === 'PUT')
  expect(put[0]).toBe('/api/admin/settings')
  expect(put[1].credentials).toBe('include')
  const payload = JSON.parse(put[1].body)
  expect(Object.keys(payload)).toHaveLength(9)
  expect(payload).toMatchObject({ settings_version: 7, ping_interval_seconds: 180,
    no_data_after_cycles: 5, idle_threshold_seconds: 900, unused_threshold_seconds: 7200 })
  await user.click(screen.getByRole('link', { name: 'Dashboard', exact: true }))
  expect(await screen.findByText('No activity for 15+ mins')).toBeInTheDocument()
  expect(screen.getByText('No activity for 2+ hours')).toBeInTheDocument()
  await user.click(screen.getByRole('link', { name: 'Settings', exact: true }))
  expect(await screen.findByLabelText('Ping interval (seconds)')).toHaveValue('180')
  expect(screen.getByLabelText('Missed report cycles before no data')).toHaveValue('5')
})

it('blocks invalid reporting fields without a PUT', async () => {
  useRealApi()
  renderWithMantine(<SettingsPage />)
  await screen.findByLabelText('Ping interval (seconds)')
  edit('Ping interval (seconds)', '0')
  edit('Missed report cycles before no data', '0')
  expect(screen.getByRole('button', { name: 'Save', exact: true })).toBeDisabled()
  expect(screen.getAllByText('Enter a whole number from 1 to 2147483647.')).toHaveLength(2)
  expect(fetch.mock.calls.some(call => call[1]?.method === 'PUT')).toBe(false)
})

it('does not present sample watts or an editable tariff timestamp as live settings', async () => {
  useRealApi()
  renderWithMantine(<SettingsPage />)
  expect(await screen.findByText(/current admin API does not expose energy rates/)).toBeInTheDocument()
  expect(screen.queryByLabelText('desktop active watts')).not.toBeInTheDocument()
  expect(screen.queryByRole('button', { name: 'Save energy rates' })).not.toBeInTheDocument()
  expect(screen.queryByLabelText('Tariff valid from (UTC)')).not.toBeInTheDocument()
  expect(screen.getByText(/Saving a new price or CO2 factor starts a tariff period/)).toBeInTheDocument()
})

it('edits the tariff date in sample mode without an API request', async () => {
  vi.stubGlobal('fetch', vi.fn())
  const user = userEvent.setup()
  renderWithMantine(<SettingsPage />)
  await screen.findByLabelText('Tariff valid from (UTC)')
  edit('Tariff valid from (UTC)', '2026-11-17T14:30')
  await user.click(screen.getByRole('button', { name: 'Save', exact: true }))
  await screen.findByText('Saved', { exact: true })
  expect((await getSettings()).tariff_valid_from).toBe('2026-11-17T14:30:00.000Z')
  expect(fetch).not.toHaveBeenCalled()
})

it('saves separate sample watts and retains them when the section remounts', async () => {
  vi.stubGlobal('fetch', vi.fn())
  const user = userEvent.setup()
  const view = renderWithMantine(<EnergyRatesSettings />)
  edit('desktop active watts', '60.5')
  expect(readSampleRates()[0].active_watts).toBe(55)
  await user.click(screen.getByRole('button', { name: 'Save energy rates' }))
  expect(screen.getByText('Energy rates saved')).toBeInTheDocument()
  expect(readSampleRates()[0].active_watts).toBe(60.5)
  view.unmount()
  renderWithMantine(<EnergyRatesSettings />)
  expect(screen.getByLabelText('desktop active watts')).toHaveValue('60.5')
  expect(fetch).not.toHaveBeenCalled()
})

it('blocks blank and negative sample watts', async () => {
  renderWithMantine(<EnergyRatesSettings />)
  edit('desktop active watts', '-1')
  expect(screen.getByRole('button', { name: 'Save energy rates' })).toBeDisabled()
  expect(screen.getByText('Enter watts of 0 or more.')).toBeInTheDocument()
  edit('desktop active watts', '')
  await waitFor(() => expect(screen.getByRole('button', { name: 'Save energy rates' })).toBeDisabled())
})
