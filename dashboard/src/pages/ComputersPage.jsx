// Full computer-list page, rendered by App.jsx for #computers.
// Loads devices through the shared provider hook, then filters and sorts them in memory.
import { useMemo, useState } from 'react'
import DataState from '../components/DataState.jsx'
import useSampleData from '../useSampleData.js'
import { Group, NativeSelect, Stack, Switch, TextInput } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'
import ComputerTable from '../components/ComputerTable.jsx'
import { getDevices } from '../data/index.js'

const STATE_OPTIONS = [
  { value: 'all', label: 'All states' },
  { value: 'active', label: 'Active' },
  { value: 'idle', label: 'Idle' },
  { value: 'unused', label: 'Unused' },
  { value: 'no_data', label: 'No data' },
]

// Match the same fallback text that the table displays for unnamed devices.
/**
 * Gives the name the table shows for a device.
 * @param {object} computer - Device from the device-list contract.
 * @returns {string} display_name, or the first eight characters of device_id.
 */
function displayedName(computer) {
  return computer.display_name ?? computer.device_id.slice(0, 8)
}

/**
 * Full computer list with search, state filter, archive switch and sortable headers.
 * @returns {JSX.Element} The page.
 */
export default function ComputersPage() {
  const { data, error, retry } = useSampleData(getDevices)
  const devices = data?.devices ?? []
  const [search, setSearch] = useState('')
  const [stateFilter, setStateFilter] = useState('all')
  const [showArchived, setShowArchived] = useState(false)
  const [sortBy, setSortBy] = useState('name')
  const [sortDirection, setSortDirection] = useState('ascending')

  const visibleDevices = useMemo(() => selectVisibleDevices(
    devices, search, stateFilter, showArchived, sortBy, sortDirection,
  ), [devices, search, stateFilter, showArchived, sortBy, sortDirection])

  /**
   * Changes the requested sort without losing filters.
   * @param {string} field - Selected name or state column.
   * @returns {void} Updates the sort state.
   */
  function handleSort(field) {
    if (sortBy === field) {
      setSortDirection(toggleDirection)
      return
    }
    setSortBy(field)
    setSortDirection('ascending')
  }

  if (error || !data) return <DataState error={error} loading={!data} onRetry={retry} label="computers" />

  return (
    <Stack gap="md">
      <Header title="Computers"
        subtitle={`Showing ${visibleDevices.length} of ${devices.filter((computer) => showArchived || !computer.archived).length} monitored computers.`}
        generatedAt={sampleScreen.generated_at} userName={sampleScreen.user_name} />
      <Group align="end" gap="md" wrap="wrap">
        <TextInput label="Search by computer name" placeholder="Search computers"
          aria-label="Search computers" value={search} onChange={(event) => setSearch(event.currentTarget.value)} />
        <NativeSelect label="Filter by state" aria-label="Filter by state"
          data={STATE_OPTIONS} value={stateFilter}
          onChange={(event) => setStateFilter(event.currentTarget.value)} />
        <Switch label="Show archived" checked={showArchived}
          onChange={(event) => setShowArchived(event.currentTarget.checked)} />
      </Group>
      {visibleDevices.length ? <ComputerTable title="All Computers" computers={visibleDevices}
        referenceTime={sampleScreen.generated_at} showArchived={showArchived}
        sortBy={sortBy} sortDirection={sortDirection} onSort={handleSort} /> : <DataState empty label="Computers" />}
    </Stack>
  )
}

/**
 * Filters the loaded devices and sorts a copy so the provider data stays unchanged.
 * @param {object[]} devices - Loaded computers.
 * @param {string} search - Displayed-name query.
 * @param {string} stateFilter - State key or all.
 * @param {boolean} showArchived - Whether archived rows are included.
 * @param {string} sortBy - Name or state column.
 * @param {string} sortDirection - Ascending or descending.
 * @returns {object[]} Matching rows in display order.
 */
function selectVisibleDevices(devices, search, stateFilter, showArchived, sortBy, sortDirection) {
  const query = search.trim().toLocaleLowerCase()
  return devices
    .filter(computer => showArchived || !computer.archived)
    .filter(computer => stateFilter === 'all' || computer.state_name === stateFilter)
    .filter(computer => displayedName(computer).toLocaleLowerCase().includes(query))
    .sort((left, right) => compareComputers(left, right, sortBy, sortDirection))
}

/**
 * Compares two rows using the selected column and direction.
 * @param {object} left - First row.
 * @param {object} right - Second row.
 * @param {string} sortBy - Name or state column.
 * @param {string} direction - Ascending or descending.
 * @returns {number} Locale comparison in the requested direction.
 */
function compareComputers(left, right, sortBy, direction) {
  const leftValue = sortBy === 'name' ? displayedName(left) : left.state_name
  const rightValue = sortBy === 'name' ? displayedName(right) : right.state_name
  const comparison = leftValue.localeCompare(rightValue, 'en', { sensitivity: 'base' })
  return direction === 'ascending' ? comparison : -comparison
}

/** @param {string} current - Sort direction. @returns {string} The opposite direction. */
function toggleDirection(current) {
  return current === 'ascending' ? 'descending' : 'ascending'
}
