// Computer list card: a table of devices from the device-list contract, with optional
// sorting, archive filter and row limit. DashboardPage shows a five-row preview, ComputersPage the full list.
import { Anchor, Button, Card, Group, Table, Text } from '@mantine/core'
import StateBadge from '../StateBadge.jsx'
import { timeAgo } from '../data/units.js'

const headerStyle = {
  color: 'var(--ews-text-muted)',
  fontWeight: 500,
  fontSize: 'var(--ews-text-sm)',
}

/**
 * Renders a device row using fields available in the device-list contract.
 * @param {object} props - Device and reference timestamp.
 * @returns {JSX.Element} Rendered content.
 */
function ComputerRow({ computer, referenceTime }) {
  const name = computer.display_name ?? computer.device_id.slice(0, 8)
  const location = [computer.location_name, computer.building].filter(Boolean).join(', ') || 'not set'

  return (
    <Table.Tr>
      <Table.Td>{name}</Table.Td>
      <Table.Td><StateBadge state={computer.state_name} /></Table.Td>
      <Table.Td>
        <Text span title={computer.last_seen_at}>
          {timeAgo(computer.last_seen_at, referenceTime)}
        </Text>
      </Table.Td>
      <Table.Td visibleFrom="sm">{location}</Table.Td>
      <Table.Td visibleFrom="sm">{computer.os} / {computer.device_type}</Table.Td>
      <Table.Td>
        <Button component="a" href={`#computers/${computer.device_id}`}
          aria-label={`View ${name}`} variant="default" size="xs"
          bg="var(--ews-button)" fw={500}>
          View
        </Button>
      </Table.Td>
    </Table.Tr>
  )
}

// Sorting controls are optional so the dashboard's compact preview stays static.
/**
 * Renders the computer list with textual status, relative last activity, location and type.
 * @param {object} props - Computers, optional title and reference timestamp; showArchived keeps
 *   archived devices, limit cuts the rows, showAllLink adds the link to the full list;
 *   sortBy, sortDirection and onSort turn the Name and Status headers into sort buttons.
 * @returns {JSX.Element} Rendered content.
 */
export default function ComputerTable({
  computers, title = 'Computer List', referenceTime,
  showArchived = false, limit, showAllLink = false,
  sortBy, sortDirection = 'ascending', onSort,
}) {
  const visible = computers.filter((computer) => showArchived || !computer.archived)
  const rows = limit === undefined ? visible : visible.slice(0, limit)
  // Plain header label, or a sort button when the page passes onSort.
  const sortLabel = (field, label) => {
    if (!onSort) return label
    const active = sortBy === field
    const direction = active ? (sortDirection === 'ascending' ? 'ascending' : 'descending') : 'none'
    return (
      <Button variant="subtle" color="gray" size="compact-sm" px={4}
        aria-label={`Sort by ${label}`} aria-pressed={active}
        onClick={() => onSort(field)}>
        {label}{active ? (sortDirection === 'ascending' ? ' ↑' : ' ↓') : ''}
      </Button>
    )
  }

  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)">
      <Group justify="space-between" mb="sm">
        <Text fz="lg" fw={700}>{title}</Text>
        {showAllLink && <Anchor href="#computers" fz="sm">All computers</Anchor>}
      </Group>
      <Table.ScrollContainer minWidth={520} type="native" role="region" aria-label={title} tabIndex={0}>
        <Table
          verticalSpacing="sm"
          horizontalSpacing="sm"
          fz="sm"
          styles={{ th: headerStyle, thead: { backgroundColor: 'var(--ews-page)' }, tr: { borderColor: 'var(--ews-border)' } }}
        >
          <Table.Thead>
            <Table.Tr>
              <Table.Th aria-sort={sortBy === 'name' ? sortDirection : 'none'}>{sortLabel('name', 'Computer Name')}</Table.Th>
              <Table.Th aria-sort={sortBy === 'state_name' ? sortDirection : 'none'}>{sortLabel('state_name', 'Status')}</Table.Th>
              <Table.Th>Last seen</Table.Th>
              <Table.Th visibleFrom="sm">Location</Table.Th>
              <Table.Th visibleFrom="sm">OS and type</Table.Th>
              <Table.Th>Actions</Table.Th>
            </Table.Tr>
          </Table.Thead>
          <Table.Tbody>
            {rows.length ? rows.map((computer) => (
              <ComputerRow key={computer.device_id} computer={computer} referenceTime={referenceTime} />
            )) : (
              <Table.Tr><Table.Td colSpan={6}><Text c="dimmed" ta="center">No computers match these filters.</Text></Table.Td></Table.Tr>
            )}
          </Table.Tbody>
        </Table>
      </Table.ScrollContainer>
    </Card>
  )
}
