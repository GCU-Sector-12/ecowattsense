import { Card, Group, SegmentedControl, Text, useMantineColorScheme } from '@mantine/core'
import { Monitor, Moon, Sun } from 'lucide-react'
import SchemeLabel from './SchemeLabel.jsx'

// Theme options. "auto" follows the colour scheme of the operating system.
const OPTIONS = [
  { value: 'light', label: <SchemeLabel icon={Sun} text="Light" /> },
  { value: 'dark', label: <SchemeLabel icon={Moon} text="Dark" /> },
  { value: 'auto', label: <SchemeLabel icon={Monitor} text="Auto" /> },
]

// "Appearance" card: lets the user change the theme. Mantine saves the choice in the browser.
export default function AppearanceCard() {
  const { colorScheme, setColorScheme } = useMantineColorScheme()

  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)">
      <Text fz="lg" fw={700} mb="md">
        Appearance
      </Text>
      <Group justify="space-between" wrap="nowrap" gap="xl">
        <div>
          <Text fz="md" fw={500}>
            Theme
          </Text>
          <Text fz="sm" c="var(--ews-text-muted)">
            Choose a light or dark look. Auto uses the setting of your computer.
          </Text>
        </div>
        <SegmentedControl value={colorScheme} onChange={setColorScheme} data={OPTIONS} />
      </Group>
    </Card>
  )
}
