import { useEffect, useState } from 'react'
import { Stack } from '@mantine/core'
import Header from '../components/Header.jsx'
import AppearanceCard from '../components/AppearanceCard.jsx'
import { getSettings } from '../data/index.js'

export default function SettingsPage() {
  const [settings, setSettings] = useState(null)
  useEffect(() => {
    let mounted = true
    getSettings().then((value) => { if (mounted) setSettings(value) })
    return () => { mounted = false }
  }, [])

  return (
    <Stack gap="md">
      <Header title="Settings"
        subtitle="Change how EcoWattSense looks."
        generatedAt="2025-03-10T14:32:00Z" userName="Admin" />
      <AppearanceCard />
      {settings && <div>Report interval: {settings.report_interval_seconds} seconds</div>}
    </Stack>
  )
}
