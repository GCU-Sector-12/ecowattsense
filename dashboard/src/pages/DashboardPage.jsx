// Dashboard page built from docs/design/dashboard-mockup.png with sample data.
// App.jsx renders this page for #dashboard; providers supply counts, energy, devices and settings.
import useSampleData from '../useSampleData.js'
import { Grid, SimpleGrid, Stack, Text } from '@mantine/core'
import { Monitor, Clock, Power } from 'lucide-react'
import Header from '../components/Header.jsx'
import StatCard from '../components/StatCard.jsx'
import EnergyCard from '../components/EnergyCard.jsx'
import EnergyChart from '../components/EnergyChart.jsx'
import StatusDonut from '../components/StatusDonut.jsx'
import ComputerTable from '../components/ComputerTable.jsx'
import { getSummary, getDevices, getSettings } from '../data/index.js'
import { sampleScreen, stateDisplay } from '../data/ui.js'

/**
 * Formats an inactivity threshold from settings as a caption, for example "10+ mins" or "1+ hour".
 * @param {number} seconds - Threshold in seconds from the settings endpoint.
 * @returns {string} Caption text, so no fixture value is baked into the UI.
 */
function formatThreshold(seconds) {
  if (seconds % 3600 === 0) {
    const hours = seconds / 3600
    return `${hours}+ ${hours === 1 ? 'hour' : 'hours'}`
  }
  if (seconds % 60 === 0) {
    const minutes = seconds / 60
    return `${minutes}+ mins`
  }
  return `${seconds}+ secs`
}

/**
 * Loads the summary, devices and settings together using a stable hook dependency.
 * @returns {Promise<Array>} Summary, device-list and settings responses.
 */
function loadDashboard() {
  return Promise.all([getSummary(), getDevices(), getSettings()])
}

/**
 * Dashboard built from docs/design/dashboard-mockup.png with sample data.
 * @returns {JSX.Element} The page.
 */
export default function DashboardPage() {
  const { data, error } = useSampleData(loadDashboard)
  if (error) return <Text role="alert">Unable to load dashboard.</Text>
  if (!data) return <Text>Loading dashboard...</Text>
  const [summary, { devices }, settings] = data
  const referenceTime = sampleScreen.generated_at
  const cards = [
    { state: 'active', icon: Monitor, caption: stateDisplay.active.caption },
    { state: 'idle', icon: Clock, caption: `No activity for ${formatThreshold(settings.idle_threshold_seconds)}` },
    { state: 'unused', icon: Power, caption: `No activity for ${formatThreshold(settings.unused_threshold_seconds)}` },
  ]

  return (
    <Stack gap="md">
      <Header title="Welcome to EcoWattSense!"
        subtitle="Here's an overview of your computers and energy usage."
        generatedAt={referenceTime} userName={sampleScreen.user_name} />
      <SimpleGrid cols={4} spacing="md">
        {cards.map(({ state, icon, caption }) => (
          <StatCard key={state} state={state} icon={icon}
            count={summary.counts[state]} total={summary.counts.total}
            label={stateDisplay[state].label} caption={caption} />
        ))}
        <EnergyCard energy_uj={summary.energy_uj} cost_gbp={summary.cost_gbp}
          co2_kg={summary.co2_kg} price_gbp_per_kwh={summary.price_gbp_per_kwh}
          co2_kg_per_kwh={summary.co2_kg_per_kwh} />
      </SimpleGrid>
      <Text c="var(--ews-text-muted)" size="sm" aria-live="polite">
        {summary.counts.no_data} {summary.counts.no_data === 1 ? 'computer' : 'computers'} with no data
      </Text>
      <Grid gutter="md">
        <Grid.Col span={8}><EnergyChart points={summary.series} /></Grid.Col>
        <Grid.Col span={4}><StatusDonut summary={summary} /></Grid.Col>
      </Grid>
      <ComputerTable computers={devices} limit={5} showAllLink referenceTime={referenceTime} />
    </Stack>
  )
}
