import { Avatar, Group, Menu, Text, Title, UnstyledButton } from '@mantine/core'
import { ChevronDown, LogOut, User } from 'lucide-react'

// Formats an ISO date as "Mon, 10 Mar 2025 14:32" (UTC, as in the mockup).
function formatDate(iso) {
  const date = new Date(iso)
  const day = date.toLocaleDateString('en-GB', { weekday: 'short', timeZone: 'UTC' })
  const rest = date.toLocaleDateString('en-GB', {
    day: '2-digit',
    month: 'short',
    year: 'numeric',
    timeZone: 'UTC',
  })
  const time = date.toLocaleTimeString('en-GB', {
    hour: '2-digit',
    minute: '2-digit',
    timeZone: 'UTC',
  })
  return `${day}, ${rest} ${time}`
}

function UserMenu({ userName }) {
  return (
    <Menu shadow="md" width={180} position="bottom-end">
      <Menu.Target>
        <UnstyledButton>
          <Group gap="xs" wrap="nowrap">
            <Avatar radius="xl" color="gray" size={40} />
            <Text fz="sm" fw={500}>
              {userName}
            </Text>
            <ChevronDown size={16} color="var(--ews-text-muted)" />
          </Group>
        </UnstyledButton>
      </Menu.Target>
      <Menu.Dropdown>
        <Menu.Item leftSection={<User size={16} />}>Profile</Menu.Item>
        <Menu.Item leftSection={<LogOut size={16} />}>Log out</Menu.Item>
      </Menu.Dropdown>
    </Menu>
  )
}

// Page header: title and subtitle on the left, date and user menu on the right.
export default function Header({ title, subtitle, generatedAt, userName }) {
  return (
    <Group justify="space-between" align="flex-start" wrap="nowrap">
      <div>
        <Title order={1} fz="xl" fw={700} lh={1.3}>
          {title}
        </Title>
        <Text c="var(--ews-text-muted)" fz="sm">
          {subtitle}
        </Text>
      </div>

      <Group gap="xl" wrap="nowrap">
        <Text fz="sm" fw={500}>
          {formatDate(generatedAt)}
        </Text>
        <UserMenu userName={userName} />
      </Group>
    </Group>
  )
}
