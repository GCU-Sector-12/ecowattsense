// Settings page, rendered by App.jsx for #settings.
// Keeps appearance controls available while the sample report interval loads.
import DataState from '../components/DataState.jsx'
import useSampleData from '../useSampleData.js'
import { Stack, Text } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'
import AppearanceCard from '../components/AppearanceCard.jsx'
import { getSettings } from '../data/index.js'

/**
 * Settings page with appearance controls and the sample report interval.
 * @returns {JSX.Element} The page.
 */
export default function SettingsPage() {
  const { data: settings, error, retry } = useSampleData(getSettings)

  return (
    <Stack gap="md">
      <Header title="Settings"
        subtitle="Change how EcoWattSense looks."
        generatedAt={sampleScreen.generated_at} userName={sampleScreen.user_name} />
      <AppearanceCard />
      <DataState error={error} loading={!settings} onRetry={retry} label="settings" />
      {settings && <Text>Report interval: {settings.report_interval_seconds} seconds</Text>}
    </Stack>
  )
}
