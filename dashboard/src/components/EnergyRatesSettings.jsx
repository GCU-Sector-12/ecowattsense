// Energy-rate section for SettingsPage. Sample edits are separate from the real settings PUT contract.
import { useEffect, useState } from 'react'
import { Button, Card, Group, NumberInput, Stack, Table, Text } from '@mantine/core'
import { isRealSettings } from '../data/settingsProvider.js'
import { readSampleRates, saveSampleRates, validateRates, WATT_FIELDS } from '../data/sampleEnergyRates.js'

/**
 * Shows editable sample rates or an explicit live API limitation without invented live watts.
 * @returns {JSX.Element} Energy rate section.
 */
export default function EnergyRatesSettings() {
  const [base, setBase] = useState(readSampleRates)
  const [rates, setRates] = useState(readSampleRates)
  const [saved, setSaved] = useState(null)
  const errors = validateRates(rates)
  const changed = JSON.stringify(base) !== JSON.stringify(rates)
  useEffect(() => expireNotice(saved, setSaved), [saved])

  /** @param {number} row - Edited row. @param {string} field - Watt name. @param {number|string} value - Input. @returns {void} Edits a draft only. */
  function change(row, field, value) {
    setRates(current => updateRate(current, row, field, value))
    setSaved(null)
  }

  /** @returns {void} Commits validated sample rows without a network request. */
  function save() {
    const result = saveSampleRates(rates)
    setBase(result)
    setRates(result)
    setSaved(result)
  }

  if (isRealSettings()) return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" style={{ '--mantine-color-error': 'var(--ews-text-muted)' }}>
      <Text component="h2" fz="lg" fw={700} m={0} mb="sm">Energy rates</Text>
      <Text>The current admin API does not expose energy rates. Live watts cannot be viewed or edited here yet.</Text>
    </Card>
  )
  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" style={{ '--mantine-color-error': 'var(--ews-text-muted)' }}>
      <Stack gap="md">
        <Text component="h2" fz="lg" fw={700} m={0}>Energy rates</Text>
        <Text size="sm" c="var(--ews-text-muted)">Sample watts are saved separately for this tab. A refresh restores the defaults.</Text>
        <Table.ScrollContainer minWidth={640} type="native" role="region" aria-label="Sample energy rates" tabIndex={0}>
          <Table aria-label="Sample watts per device type">
            <Table.Thead><Table.Tr><Table.Th>Device type</Table.Th>
              {WATT_FIELDS.map(field => <Table.Th key={field}>{field.replace('_watts', '')} (W)</Table.Th>)}
            </Table.Tr></Table.Thead>
            <Table.Tbody>{rates.map((rate, row) => (
              <Table.Tr key={rate.device_type}>
                <Table.Td>{rate.device_type}</Table.Td>
                {WATT_FIELDS.map(field => <Table.Td key={field}>
                  <NumberInput aria-label={`${rate.device_type} ${field.replace('_', ' ')}`}
                    value={rate[field]} hideControls error={errors[`${row}.${field}`]}
                    onChange={value => change(row, field, value)} />
                </Table.Td>)}
              </Table.Tr>
            ))}</Table.Tbody>
          </Table>
        </Table.ScrollContainer>
        <Group>
          <Button variant="default" onClick={save} disabled={!changed || Object.keys(errors).length > 0}>Save energy rates</Button>
          {saved && <Text role="status">Energy rates saved</Text>}
        </Group>
      </Stack>
    </Card>
  )
}

/** @param {object[]} rates - Draft rows. @param {number} row - Edited row. @param {string} field - Watt name. @param {*} value - Input. @returns {object[]} New draft rows. */
function updateRate(rates, row, field, value) {
  return rates.map((rate, index) => index === row ? { ...rate, [field]: value } : rate)
}

/** @param {object|null} saved - Successful save identity. @param {Function} setSaved - Status setter. @returns {Function|undefined} Three-second timer cleanup. */
function expireNotice(saved, setSaved) {
  if (!saved) return undefined
  const timer = setTimeout(() => setSaved(null), 3000)
  return () => clearTimeout(timer)
}
