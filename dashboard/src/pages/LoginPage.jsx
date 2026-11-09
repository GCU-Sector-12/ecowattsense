// Sample login page used by App.jsx when no demo session exists.
import { useState } from 'react'
import { Alert, Button, PasswordInput, Stack, Text, TextInput, Title } from '@mantine/core'
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

  return (
    <Stack maw={420} mx="auto" mt={100} gap="md">
      <Title order={1}>EcoWattSense</Title>
      <Text c="dimmed">Log in to access the dashboard.</Text>
      <form onSubmit={handleSubmit}>
        <Stack gap="md">
          <TextInput label="Username" name="username" autoComplete="username" value={username}
            onChange={(event) => setUsername(event.currentTarget.value)} required />
          <PasswordInput label="Password" aria-label="Password" name="password" autoComplete="current-password" value={password}
            onChange={(event) => setPassword(event.currentTarget.value)} required />
          {error && <Alert color="red" role="alert">{error}</Alert>}
          <Button type="submit" fullWidth>Log in</Button>
        </Stack>
      </form>
    </Stack>
  )
}
