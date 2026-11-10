// Shared dashboard header with the signed-in demo user menu.
import { Avatar, Group, Menu, Text, Title, UnstyledButton } from '@mantine/core'
import { ChevronDown, LogOut, User } from 'lucide-react'
import { clearSampleSession, getSampleSession } from '../auth/sampleSession.js'

/**
 * Formats an ISO date as a UTC date and time for the dashboard header.
 * @param {string} iso - ISO date string.
 * @returns {string} Formatted date and time.
 */
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

/**
 * Shows the current username and provides the demo logout action.
 * @param {{userName: string}} props - Signed-in username.
 * @returns {JSX.Element} User menu.
 */
function UserMenu({ userName }) {
  const sessionUser = getSampleSession()?.username || userName
  /**
   * Ends the demo session and returns the browser to the login page.
   * @returns {void}
   */
  function handleLogout() {
    clearSampleSession()
    window.location.hash = '#login'
  }

  return (
    <Menu shadow="md" width={180} position="bottom-end">
      <Menu.Target>
        <UnstyledButton aria-label={`User menu for ${sessionUser}`}>
          <Group gap="xs" wrap="nowrap">
            <Avatar radius="xl" color="gray" size={40} />
            <Text fz="sm" fw={500}>{sessionUser}</Text>
            <ChevronDown size={16} color="var(--ews-text-muted)" />
          </Group>
        </UnstyledButton>
      </Menu.Target>
      <Menu.Dropdown>
        <Menu.Item leftSection={<User size={16} />}>Profile</Menu.Item>
        <Menu.Item leftSection={<LogOut size={16} />} onClick={handleLogout}>Log out</Menu.Item>
      </Menu.Dropdown>
    </Menu>
  )
}

/**
 * Renders a page title, subtitle, timestamp and signed-in user menu.
 * @param {{title: string, subtitle: string, generatedAt: string, userName: string}} props - Header content.
 * @returns {JSX.Element} Dashboard header.
 */
export default function Header({ title, subtitle, generatedAt, userName }) {
  return (
    <Group justify="space-between" align="flex-start" wrap="nowrap">
      <div>
        <Title order={1} fz="xl" fw={700} lh={1.3}>{title}</Title>
        <Text c="var(--ews-text-muted)" fz="sm">{subtitle}</Text>
      </div>
      <Group gap="xl" wrap="nowrap">
        <Text fz="sm" fw={500}>{formatDate(generatedAt)}</Text>
        <UserMenu userName={userName} />
      </Group>
    </Group>
  )
}
