// Settings page, rendered by App.jsx for /settings.
// Shows the settings form (real admin settings or sample data) and the appearance controls.
import DataState from '../components/DataState.jsx'
import useSampleData from '../useSampleData.js'
import { Card, Stack, Switch, Text } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'
import AppearanceCard from '../components/AppearanceCard.jsx'
import SettingsForm from '../components/SettingsForm.jsx'
import { getSettings } from '../data/index.js'
import { isRealSettings } from '../data/settingsProvider.js'
import EnergyRatesSettings from '../components/EnergyRatesSettings.jsx'
import { useState } from 'react'
import { readDeveloperMode, saveDeveloperMode } from '../data/developerMode.js'
import classes from './SettingsPage.module.css'

/**
 * Settings page with the settings form and appearance controls.
 * The header shows the current time in real mode and the sample time otherwise.
 * @returns {JSX.Element} The page.
 */
export default function SettingsPage() {
  const { data: settings, error, retry } = useSampleData(getSettings, { enabled: false })
  const [developerMode, setDeveloperMode] = useState(readDeveloperMode)
  const generatedAt = isRealSettings() ? new Date().toISOString() : sampleScreen.generated_at

  /** @param {React.ChangeEvent<HTMLInputElement>} event - Updated switch input. @returns {void} Saves the browser preference. */
  function changeDeveloperMode(event) {
    const enabled = event.currentTarget.checked
    saveDeveloperMode(enabled)
    setDeveloperMode(enabled)
  }

  return (
    <Stack gap="md">
      <Header title="Settings"
        subtitle="Change how the system works and how EcoWattSense looks."
        generatedAt={generatedAt} userName={sampleScreen.user_name} />
      <Card radius="lg" padding="lg" bg="var(--ews-card)">
        <Switch label="Developer mode" description="Show every history report field exactly as returned by the API."
          checked={developerMode} onChange={changeDeveloperMode} />
      </Card>
      <DataState error={error} loading={!settings && !error} onRetry={retry} label="settings" />
      {!isRealSettings() && <Text size="sm" c="var(--ews-text-muted)">
        Sample settings are saved for this tab only. Refreshing restores defaults; no server settings are changed.
      </Text>}
      {settings && <SettingsForm settings={settings} />}
      {settings && <EnergyRatesSettings />}
      <div className={classes.appearance}><AppearanceCard /></div>
    </Stack>
  )
}
