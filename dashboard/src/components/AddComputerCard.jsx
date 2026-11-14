// Card on the Computers page that tells a person how to add a Windows computer.
// Used by pages/ComputersPage.jsx above the computer table.
import { Button, Card, List, Stack, Text } from '@mantine/core'
import { MonitorDown } from 'lucide-react'
import { INSTALLER_DOWNLOAD_URL } from '../data/ui.js'

/**
 * Shows three numbered steps, the server address and the installer download link.
 * The server address is the dashboard's own origin, because the dashboard and the
 * server are served from the same place.
 * @returns {JSX.Element} The card.
 */
export default function AddComputerCard() {
  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card-alt)">
      <Stack gap="sm">
        <Text fz="lg" fw={600}>Add a Windows computer</Text>
        <List type="ordered" spacing="xs">
          <List.Item>Download and run the installer.</List.Item>
          <List.Item>
            Enter this server address and the key.
            <Text component="div" fz="sm" ff="monospace" data-testid="server-address">
              {window.location.origin}
            </Text>
          </List.Item>
          <List.Item>The computer shows here within 5 minutes.</List.Item>
        </List>
        <Button component="a" href={INSTALLER_DOWNLOAD_URL} leftSection={<MonitorDown size={18} />}
          w="fit-content">
          Download installer
        </Button>
        <Text fz="sm" c="var(--ews-text-muted)">Windows 11, 64-bit.</Text>
      </Stack>
    </Card>
  )
}
