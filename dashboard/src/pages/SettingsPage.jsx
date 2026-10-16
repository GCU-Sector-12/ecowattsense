import { Stack } from '@mantine/core'
import Header from '../components/Header.jsx'
import AppearanceCard from '../components/AppearanceCard.jsx'

// Settings page. For now it has one section, Appearance (theme).
export default function SettingsPage({ data }) {
  return (
    <Stack gap="md">
      <Header
        title="Settings"
        subtitle="Change how EcoWattSense looks."
        generatedAt={data.generatedAt}
        userName={data.user.name}
      />
      <AppearanceCard />
    </Stack>
  )
}
