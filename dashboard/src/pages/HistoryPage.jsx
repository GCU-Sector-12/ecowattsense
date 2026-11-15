// Daily computer history, opened from the computer table through #computers/<device_id>.
import { useCallback, useState } from 'react'
import { Anchor, Box, Card, SimpleGrid, Stack, Text, TextInput } from '@mantine/core'
import Header from '../components/Header.jsx'
import DataState from '../components/DataState.jsx'
import EnergyChart from '../components/EnergyChart.jsx'
import HistoryReports from '../components/HistoryReports.jsx'
import useSampleData from '../useSampleData.js'
import { sampleScreen } from '../data/ui.js'
import { formatGbp, timeAgo, ujToKwh } from '../data/units.js'
import { isRealHistory } from '../data/historyProvider.js'
import {
  calendarDay, energyByHalfHour, formatDuration, historyDate, loadComputerHistory, SAMPLE_HISTORY_FROM, shiftDay, shortDay,
} from '../data/historyView.js'

/**
 * Shows one labelled computer detail using description-list semantics.
 * @param {object} props - Detail label and value.
 * @returns {JSX.Element} Description-list entry.
 */
function DeviceDetail({ label, value }) {
  return (
    <Box>
      <Text component="dt" size="sm" c="var(--ews-text-muted)">{label}</Text>
      <Text component="dd" m={0} style={{ overflowWrap: 'anywhere' }}>{value}</Text>
    </Box>
  )
}

/**
 * Shows identity fields from the device provider, rather than borrowing another device's details.
 * @param {object} props - Loaded device and the time that Last seen is measured against.
 * @returns {JSX.Element} Computer identity card.
 */
function DeviceDetails({ device, referenceTime }) {
  const location = [device.location_name, device.building].filter(Boolean).join(', ') || 'Not set'
  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)">
      <Text component="h2" fz="lg" fw={700} m={0}>{device.display_name ?? device.device_id}</Text>
      <SimpleGrid component="dl" cols={{ base: 1, sm: 2, lg: 3 }} mb={0}>
        <DeviceDetail label="Device ID" value={device.device_id} />
        <DeviceDetail label="Operating system" value={device.os} />
        <DeviceDetail label="Device type" value={device.device_type} />
        <DeviceDetail label="Location" value={location} />
        <DeviceDetail label="Last seen" value={<time dateTime={device.last_seen_at} title={device.last_seen_at}>
          {timeAgo(device.last_seen_at, referenceTime)}
        </time>} />
      </SimpleGrid>
    </Card>
  )
}

/**
 * Shows all six totals with durations, converted kWh, GBP and CO2 units.
 * @param {object} props - History totals using API field names.
 * @returns {JSX.Element} Responsive daily totals row.
 */
function HistoryTotals({ totals }) {
  const entries = [
    ['Active time', formatDuration(totals.active_time_seconds)],
    ['Idle time', formatDuration(totals.idle_time_seconds)],
    ['Unused time', formatDuration(totals.unused_time_seconds)],
    ['Energy', `${ujToKwh(totals.energy_uj).toFixed(3)} kWh`],
    ['Cost', formatGbp(totals.cost_gbp)],
    ['CO₂', `${totals.co2_kg.toFixed(2)} kg`],
  ]
  return (
    <SimpleGrid cols={{ base: 2, sm: 3, xl: 6 }} spacing="md" aria-label="Daily totals">
      {entries.map(([label, value]) => (
        <Card key={label} component="section" aria-label={label} radius="lg" padding="md" bg="var(--ews-card)">
          <Text size="sm" c="var(--ews-text-muted)">{label}</Text>
          <Text fw={700} fz="lg">{value}</Text>
        </Card>
      ))}
    </SimpleGrid>
  )
}

/**
 * Separates a missing device from a known computer without sample reports.
 * @param {object} props - Loaded device and optional history response.
 * @returns {JSX.Element} Details and history, or the appropriate empty state.
 */
function HistoryContent({ data, day, paging }) {
  if (!data.device) return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)">
      <Text component="h2" fz="lg" fw={700}>Device not found</Text>
      <Text>This computer is not in the device list.</Text>
    </Card>
  )
  return (
    <Stack gap="md">
      <DeviceDetails device={data.device} referenceTime={isRealHistory() ? new Date() : sampleScreen.generated_at} />
      {data.history ? <AvailableHistory history={data.history} day={day} paging={paging} /> : (
        <Card radius="lg" padding="lg" bg="var(--ews-card)" role="status">
          <Text>{isRealHistory() ? 'No history available for this computer.' : 'No sample history available for this computer.'}</Text>
        </Card>
      )}
    </Stack>
  )
}

/**
 * Shows populated history or an empty-report message without labelling the device as missing.
 * @param {object} props - History response with reports and totals, the selected day and table paging props.
 * @returns {JSX.Element} Daily totals and available reports.
 */
function AvailableHistory({ history, day, paging }) {
  return (
    <Stack gap="md">
      <HistoryTotals totals={history.totals} />
      {history.reports.length ? <EnergyChart points={energyByHalfHour(history.reports)} title={`Energy usage, ${shortDay(day)}`} /> : null}
      {history.reports.length ? <HistoryReports reports={history.reports} {...paging} /> : <DataState empty label="History reports" />}
    </Stack>
  )
}

/**
 * Loads one computer's history and defaults the date input to today.
 * Real mode loads the selected UTC day; sample mode keeps the fixed sample readings whatever the date.
 * @param {object} props - Route parameters supplied by App.
 * @param {string} props.deviceId - Requested computer identifier.
 * @returns {JSX.Element} History page with loading, retry, missing and populated states.
 */
export default function HistoryPage({ deviceId }) {
  const [day, setDay] = useState(calendarDay)
  const [page, setPage] = useState(1)
  const [pageSize, setPageSize] = useState(25)
  // Sample readings do not depend on the date, so only real mode reloads when it changes.
  const loadDay = isRealHistory() ? day : undefined
  const loader = useCallback(() => loadComputerHistory(deviceId, loadDay), [deviceId, loadDay])
  const { data, error, retry } = useSampleData(loader)

  /** @param {Event} event - Native date change. @returns {void} Stores the selected date and returns to page 1. */
  function changeDay(event) {
    setDay(event.currentTarget.value || day)
    setPage(1)
  }

  /** @param {number} size - New rows per page. @returns {void} Stores the size and returns to page 1. */
  function changePageSize(size) {
    setPageSize(size)
    setPage(1)
  }
  /** @param {number} delta - -1 or 1. @returns {void} Moves the selected day and returns to page 1. */
  function shiftSelectedDay(delta) {
    setDay(shiftDay(day, delta))
    setPage(1)
  }
  // Only real mode pages the table and switches days; the sample day has 48 rows and shows them all.
  const paging = isRealHistory() ? {
    page, pageSize, onPage: setPage, onPageSize: changePageSize,
    day, onShiftDay: shiftSelectedDay, canNext: day < calendarDay(),
  } : {}
  const generatedAt = isRealHistory() ? new Date().toISOString() : sampleScreen.generated_at

  return (
    <Stack gap="md">
      <Header title="Computer history" subtitle={`Computer ${deviceId}`}
        generatedAt={generatedAt} userName={sampleScreen.user_name} />
      <Anchor href="#computers">Back to computers</Anchor>
      <TextInput type="date" label="History date" value={day} onChange={changeDay} maw={240} />
      {isRealHistory() ? null : <Text size="sm" c="var(--ews-text-muted)">
        Sample history from {historyDate(SAMPLE_HISTORY_FROM)}. Date selection does not change these sample readings.
      </Text>}
      {data ? <HistoryContent data={data} day={day} paging={paging} /> : <DataState error={error} loading={!error} onRetry={retry} label="computer history" />}
    </Stack>
  )
}
