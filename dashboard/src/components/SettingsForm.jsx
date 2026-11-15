// "Settings" card of the Settings page: report interval, Idle and Unused thresholds, price and CO2 factor.
// Reads the last settings from props and saves with PUT /api/admin/settings through saveSettings.
// Rules and the request body are in settingsForm.js.
import { useEffect, useState } from 'react'
import { Alert, Button, Card, Group, NumberInput, SimpleGrid, Stack, Text } from '@mantine/core'
import { getSettings, saveSettings } from '../data/index.js'
import { buildPayload, isChanged, toFormValues, validateForm } from './settingsForm.js'

const SAVED_MILLISECONDS = 3000
const CONFLICT_TEXT = 'Settings were changed by someone else. Reload to see them.'
const NETWORK_TEXT = 'Could not save the settings. Check the connection and try again.'

/**
 * Chooses the message and the kind of status for a failed save.
 * @param {Error & {status?: number}} error - Failure from saveSettings.
 * @returns {{kind: string, message: string}} Status shown next to the form.
 */
function failureStatus(error) {
  if (error.status === 409) return { kind: 'conflict', message: CONFLICT_TEXT }
  if (error.status === 422) return { kind: 'invalid', message: error.message }
  return { kind: 'failed', message: NETWORK_TEXT }
}

/**
 * Shows the result of the last save, with Reload for 409 and Try again for other failures.
 * @param {{status: {kind: string, message: string}, onReload: Function, onRetry: Function}} props - Status and actions.
 * @returns {JSX.Element|null} An alert, the word Saved, or nothing.
 */
function SaveStatus({ status, onReload, onRetry }) {
  if (status.kind === 'saved') return <Text c="var(--ews-text-muted)" role="status">Saved</Text>
  if (status.kind === 'invalid') return <Alert color="gray" role="alert">{status.message}</Alert>
  if (status.kind === 'conflict') return <StatusAlert message={status.message} action="Reload" onAction={onReload} />
  if (status.kind === 'failed') return <StatusAlert message={status.message} action="Try again" onAction={onRetry} />
  return null
}

/**
 * Shows an alert with one action button.
 * @param {{message: string, action: string, onAction: Function}} props - Text and the button.
 * @returns {JSX.Element} The alert.
 */
function StatusAlert({ message, action, onAction }) {
  return (
    <Alert color="gray" role="alert">
      <Group justify="space-between" wrap="nowrap">
        <Text fz="sm">{message}</Text>
        <Button variant="default" size="xs" onClick={onAction}>{action}</Button>
      </Group>
    </Alert>
  )
}

/**
 * Settings form. The Save button is disabled while saving, when nothing changed, or when a value is invalid.
 * @param {{settings: object}} props - The settings from the last GET.
 * @returns {JSX.Element} The card.
 */
export default function SettingsForm({ settings }) {
  const [base, setBase] = useState(settings)
  const [values, setValues] = useState(() => toFormValues(settings))
  const [status, setStatus] = useState({ kind: 'idle', message: '' })
  const errors = validateForm(values)
  const canSave = status.kind !== 'saving' && isChanged(values, base) && Object.keys(errors).length === 0

  useEffect(() => hideSavedLater(status.kind, setStatus), [status])

  /**
   * Builds the change handler of one field.
   * @param {string} name - Field name in the form values.
   * @returns {Function} Handler that stores the new value.
   */
  function change(name) {
    return value => setValues(current => ({ ...current, [name]: value }))
  }

  /** @returns {Promise<void>} Sends the form; a good answer becomes the new form state. */
  async function save() {
    setStatus({ kind: 'saving', message: '' })
    try {
      const saved = await saveSettings(buildPayload(values, base))
      setBase(saved)
      setValues(toFormValues(saved))
      setStatus({ kind: 'saved', message: '' })
    } catch (error) {
      setStatus(failureStatus(error))
    }
  }

  /** @returns {Promise<void>} Reads the settings again and replaces the typed values. */
  async function reload() {
    try {
      const fresh = await getSettings()
      setBase(fresh)
      setValues(toFormValues(fresh))
      setStatus({ kind: 'idle', message: '' })
    } catch {
      setStatus({ kind: 'failed', message: 'Could not reload the settings. Check the connection and try again.' })
    }
  }

  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)">
      <Stack gap="md">
        <Text fz="lg" fw={700}>Settings</Text>
        <SimpleGrid cols={{ base: 1, sm: 2 }} spacing="md">
          <NumberInput label="Report interval (seconds)" value={values.report_interval_seconds}
            onChange={change('report_interval_seconds')} error={errors.report_interval_seconds}
            allowDecimal={false} hideControls />
          <div />
          <NumberInput label="Idle after (minutes)" value={values.idle_minutes}
            onChange={change('idle_minutes')} error={errors.idle_minutes} hideControls />
          <NumberInput label="Unused after (minutes)" value={values.unused_minutes}
            onChange={change('unused_minutes')} error={errors.unused_minutes} hideControls />
          <NumberInput label="Electricity price (GBP per kWh)" value={values.price_gbp_per_kwh}
            onChange={change('price_gbp_per_kwh')} error={errors.price_gbp_per_kwh} hideControls />
          <NumberInput label="CO2 (kg per kWh)" value={values.co2_kg_per_kwh}
            onChange={change('co2_kg_per_kwh')} error={errors.co2_kg_per_kwh} hideControls />
        </SimpleGrid>
        <SaveStatus status={status} onReload={reload} onRetry={save} />
        <Group>
          <Button onClick={save} disabled={!canSave} loading={status.kind === 'saving'}>Save</Button>
        </Group>
      </Stack>
    </Card>
  )
}

/**
 * Starts the timer that hides "Saved" after 3 seconds.
 * @param {string} kind - Current status kind.
 * @param {Function} setStatus - Status setter.
 * @returns {Function|undefined} Cleanup that stops the timer.
 */
function hideSavedLater(kind, setStatus) {
  if (kind !== 'saved') return undefined
  const timer = setTimeout(() => setStatus({ kind: 'idle', message: '' }), SAVED_MILLISECONDS)
  return () => clearTimeout(timer)
}
