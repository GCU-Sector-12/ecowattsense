// Renders a device row using fields available in the device-list contract.
import { Button, Card, Group, Table, Text } from '@mantine/core'
import StatusDot from './StatusDot.jsx'
import { timeAgo } from '../data/units.js'
import { stateLabels } from '../data/ui.js'

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
  return (
    <Table.Tr>
      <Table.Td>{computer.display_name}</Table.Td>
      <Table.Td>
        <Group gap="xs" wrap="nowrap">
          <StatusDot state={computer.state_name} size={14} />
          <Text fz="sm">{stateLabels[computer.state_name]}</Text>
        </Group>
      </Table.Td>
      <Table.Td>{timeAgo(computer.last_seen_at, referenceTime)}</Table.Td>
      <Table.Td>
        <Button variant="default" size="xs" bg="var(--ews-button)" fw={500}>
          View
        </Button>
      </Table.Td>
    </Table.Tr>
  )
}

// "Computer List" card with a table of computers. Status is a dot plus a word.
/**
 * Renders the computer list with textual status and relative last activity.
 * @param {object} props - Computers, optional title and reference timestamp.
 * @returns {JSX.Element} Rendered content.
 */
export default function ComputerTable({ computers, title = 'Computer List', referenceTime }) {
  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)">
      <Text fz="lg" fw={700} mb="sm">
        {title}
      </Text>
      <Table
        verticalSpacing="sm"
        horizontalSpacing="sm"
        fz="sm"
        styles={{ th: headerStyle, thead: { backgroundColor: 'var(--ews-page)' }, tr: { borderColor: 'var(--ews-border)' } }}
      >
        <Table.Thead>
          <Table.Tr>
            <Table.Th>Computer Name</Table.Th>
            <Table.Th>Status</Table.Th>
            <Table.Th>Last Activity</Table.Th>
            <Table.Th>Actions</Table.Th>
          </Table.Tr>
        </Table.Thead>
        <Table.Tbody>
          {computers.map((computer) => (
            <ComputerRow key={computer.device_id} computer={computer} referenceTime={referenceTime} />
          ))}
        </Table.Tbody>
      </Table>
    </Card>
  )
}
