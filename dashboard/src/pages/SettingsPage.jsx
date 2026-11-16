// Settings page, rendered by App.jsx for /settings.
// Shows the settings form (real admin settings or sample data) and the appearance controls.
import DataState from '../components/DataState.jsx'
import useSampleData from '../useSampleData.js'
import { Stack } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'
import AppearanceCard from '../components/AppearanceCard.jsx'
import SettingsForm from '../components/SettingsForm.jsx'
import { getSettings } from '../data/index.js'
import { isRealSettings } from '../data/settingsProvider.js'

/**
 * Settings page with the settings form and appearance controls.
 * The header shows the current time in real mode and the sample time otherwise.
 * @returns {JSX.Element} The page.
 */
export default function SettingsPage() {
  const { data: settings, error, retry } = useSampleData(getSettings)
  const generatedAt = isRealSettings() ? new Date().toISOString() : sampleScreen.generated_at

  return (
    <Stack gap="md">
      <Header title="Settings"
        subtitle="Change how the system works and how EcoWattSense looks."
        generatedAt={generatedAt} userName={sampleScreen.user_name} />
      <DataState error={error} loading={!settings && !error} onRetry={retry} label="settings" />
      {settings && <SettingsForm settings={settings} />}
      <AppearanceCard />
    </Stack>
  )
}
