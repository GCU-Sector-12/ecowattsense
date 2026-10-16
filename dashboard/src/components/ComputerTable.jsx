import { Button, Card, Group, Table, Text } from '@mantine/core'
import StatusDot from './StatusDot.jsx'

const LABELS = { active: 'Active', idle: 'Idle', unused: 'Unused' }

const headerStyle = {
  color: 'var(--ews-text-muted)',
  fontWeight: 500,
  fontSize: 'var(--ews-text-sm)',
}

function ComputerRow({ computer }) {
  return (
    <Table.Tr>
      <Table.Td>{computer.name}</Table.Td>
      <Table.Td>
        <Group gap="xs" wrap="nowrap">
          <StatusDot state={computer.status} size={14} />
          <Text fz="sm">{LABELS[computer.status]}</Text>
        </Group>
      </Table.Td>
      <Table.Td>{computer.lastActivityText}</Table.Td>
      <Table.Td>{computer.estimatedPowerW} W</Table.Td>
      <Table.Td>
        <Button variant="default" size="xs" bg="var(--ews-button)" fw={500}>
          View
        </Button>
      </Table.Td>
    </Table.Tr>
  )
}

// "Computer List" card with a table of computers. Status is a dot plus a word.
export default function ComputerTable({ computers, title = 'Computer List' }) {
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
            <Table.Th>Estimated Power</Table.Th>
            <Table.Th>Actions</Table.Th>
          </Table.Tr>
        </Table.Thead>
        <Table.Tbody>
          {computers.map((computer) => (
            <ComputerRow key={computer.id} computer={computer} />
          ))}
        </Table.Tbody>
      </Table>
    </Card>
  )
}
