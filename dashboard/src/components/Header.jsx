// Shared responsive page header with the signed-in user menu.
import { useState } from 'react'
import { Alert, Avatar, Group, Menu, Text, Title, UnstyledButton } from '@mantine/core'
import { ChevronDown, LogOut, User } from 'lucide-react'
import { logout } from '../auth/logout.js'
import { clearSampleSession, getSampleSession } from '../auth/sampleSession.js'
import classes from './Header.module.css'

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
 * Shows the current username and provides the API logout action.
 * @param {{userName: string}} props - Signed-in username.
 * @returns {JSX.Element} User menu and any logout error.
 */
function UserMenu({ userName }) {
  const sessionUser = getSampleSession()?.username || userName
  const [logoutError, setLogoutError] = useState('')
  const [isLoggingOut, setIsLoggingOut] = useState(false)

  /**
   * Ends the API session, clears the browser session and opens the login page.
   * @returns {Promise<void>} Resolves after the user is returned to login.
   * @throws {DOMException} If browser session storage cannot be cleared.
   */
  async function handleLogout() {
    setLogoutError('')
    setIsLoggingOut(true)
    try {
      await logout()
      clearSampleSession()
      window.location.hash = '#login'
    } catch (error) {
      setLogoutError(error.message || 'Could not log out. Try again.')
    } finally {
      setIsLoggingOut(false)
    }
  }

  return (
    <>
      <Menu shadow="md" width={220} position="bottom-end">
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
          <Menu.Item leftSection={<LogOut size={16} />} onClick={handleLogout} disabled={isLoggingOut}>
            Log out
          </Menu.Item>
        </Menu.Dropdown>
      </Menu>
      {logoutError && <Alert color="gray" role="alert">{logoutError}</Alert>}
    </>
  )
}

/**
 * Renders a page title, subtitle, timestamp and signed-in user menu.
 * @param {{title: string, subtitle: string, generatedAt: string, userName: string}} props - Header content.
 * @returns {JSX.Element} The responsive page header.
 */
export default function Header({ title, subtitle, generatedAt, userName }) {
  return (
    <Group className={classes.header} justify="space-between" align="flex-start">
      <div>
        <Title order={1} fz="xl" fw={700} lh={1.3}>{title}</Title>
        <Text c="var(--ews-text-muted)" fz="sm">{subtitle}</Text>
      </div>

      <Group gap={{ base: 'sm', sm: 'xl' }}>
        <Text fz="sm" fw={500}>
          {formatDate(generatedAt)}
        </Text>
        <UserMenu userName={userName} />
      </Group>
    </Group>
  )
}
