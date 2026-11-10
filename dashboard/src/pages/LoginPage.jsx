// Sample login page used by App.jsx when no demo session exists.
import { useState } from 'react'
import { Alert, Button, Center, Group, Paper, PasswordInput, Stack, Text, TextInput, Title } from '@mantine/core'
import { Leaf } from 'lucide-react'
import { sampleCredentials } from '../data/ui.js'
import { createSampleSession } from '../auth/sampleSession.js'

/**
 * Shows the demo login form and creates a session for the sample admin.
 * @param {{onLogin: (session: {username: string}) => void}} props - Session update callback.
 * @returns {JSX.Element} The login form.
 */
export default function LoginPage({ onLogin }) {
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [error, setError] = useState('')

  /**
   * Checks the demo credentials and starts a browser session when they match.
   * @param {import('react').FormEvent<HTMLFormElement>} event - Form submission.
   * @returns {void}
   * @throws {DOMException} If sessionStorage is unavailable or access is denied.
   * @throws {Error} If the onLogin callback fails.
   */
  function handleSubmit(event) {
    event.preventDefault()
    const valid = username === sampleCredentials.username && password === sampleCredentials.password
    if (!valid) {
      setError('Wrong username or password')
      return
    }
    const session = createSampleSession(username)
    onLogin(session)
  }

  /** Updates the username from the form field. */
  function handleUsernameChange(event) {
    setUsername(event.currentTarget.value)
  }

  /** Updates the password from the form field. */
  function handlePasswordChange(event) {
    setPassword(event.currentTarget.value)
  }

  return (
    <Center mih="100vh" px="md" py="xl" bg="var(--ews-page)">
      <Paper
        w="100%"
        maw={430}
        p={{ base: 'xl', sm: 40 }}
        radius="lg"
        shadow="0 12px 36px rgba(15, 23, 42, 0.08)"
        withBorder
        style={{ borderColor: 'var(--ews-border)' }}
      >
        <Stack gap="lg">
          <Stack align="center" gap="sm">
            <Group gap="xs" align="center">
              <Leaf size={40} color="var(--ews-leaf)" aria-hidden="true" />
              <Title order={2} c="var(--ews-green-900)" fz="xl">EcoWattSense</Title>
            </Group>
            <Text
              size="xs"
              fw={700}
              c="var(--ews-green-700)"
              style={{ letterSpacing: '0.12em' }}
            >
              SECURE DASHBOARD ACCESS
            </Text>
            <Title order={1} ta="center" c="var(--ews-text)">Welcome back</Title>
            <Text ta="center" c="var(--ews-text-muted)" size="sm">
              Sign in to access the EcoWattSense monitoring dashboard.
            </Text>
          </Stack>

          <form onSubmit={handleSubmit}>
            <Stack gap="md">
              <TextInput
                label="Username"
                name="username"
                placeholder="Enter username"
                autoComplete="username"
                value={username}
                onChange={handleUsernameChange}
                required
              />
              <PasswordInput
                label="Password"
                aria-label="Password"
                name="password"
                placeholder="Enter password"
                autoComplete="current-password"
                value={password}
                onChange={handlePasswordChange}
                required
              />
              {error && <Alert color="gray" role="alert">{error}</Alert>}
              <Button type="submit" fullWidth color="ews">
                Log in
              </Button>
            </Stack>
          </form>
        </Stack>
      </Paper>
    </Center>
  )
}
