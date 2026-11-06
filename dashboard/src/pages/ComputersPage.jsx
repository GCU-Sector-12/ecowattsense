import { useEffect, useMemo, useState } from 'react'
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
function displayedName(computer) {
  return computer.display_name ?? computer.device_id.slice(0, 8)
}

export default function ComputersPage() {
  const [devices, setDevices] = useState([])
  const [search, setSearch] = useState('')
  const [stateFilter, setStateFilter] = useState('all')
  const [showArchived, setShowArchived] = useState(false)
  const [sortBy, setSortBy] = useState('name')
  const [sortDirection, setSortDirection] = useState('ascending')

  useEffect(() => {
    let mounted = true
    getDevices().then((response) => { if (mounted) setDevices(response.devices) })
    return () => { mounted = false }
  }, [])

  const visibleDevices = useMemo(() => {
    const query = search.trim().toLocaleLowerCase()
    return devices
      .filter((computer) => showArchived || !computer.archived)
      .filter((computer) => stateFilter === 'all' || computer.state_name === stateFilter)
      .filter((computer) => displayedName(computer).toLocaleLowerCase().includes(query))
      .sort((left, right) => {
        const leftValue = sortBy === 'name' ? displayedName(left) : left.state_name
        const rightValue = sortBy === 'name' ? displayedName(right) : right.state_name
        const comparison = leftValue.localeCompare(rightValue, 'en', { sensitivity: 'base' })
        return sortDirection === 'ascending' ? comparison : -comparison
      })
  }, [devices, search, stateFilter, showArchived, sortBy, sortDirection])

  function handleSort(field) {
    if (sortBy === field) {
      setSortDirection((current) => current === 'ascending' ? 'descending' : 'ascending')
    } else {
      setSortBy(field)
      setSortDirection('ascending')
    }
  }

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
      <ComputerTable title="All Computers" computers={visibleDevices}
        referenceTime={sampleScreen.generated_at} showArchived={showArchived}
        sortBy={sortBy} sortDirection={sortDirection} onSort={handleSort} />
    </Stack>
  )
}
