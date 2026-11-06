// Dashboard page built from docs/design/dashboard-mockup.png with sample data.
// App.jsx renders this page for #dashboard; providers supply counts, energy and devices.
import useSampleData from '../useSampleData.js'
import { Grid, SimpleGrid, Stack, Text } from '@mantine/core'
import { Monitor, Clock, Power } from 'lucide-react'
import Header from '../components/Header.jsx'
import StatCard from '../components/StatCard.jsx'
import EnergyCard from '../components/EnergyCard.jsx'
import EnergyChart from '../components/EnergyChart.jsx'
import StatusDonut from '../components/StatusDonut.jsx'
import ComputerTable from '../components/ComputerTable.jsx'
import { getSummary, getDevices } from '../data/index.js'
import { sampleScreen, stateDisplay } from '../data/ui.js'

/**
 * Loads the summary and devices together using a stable hook dependency.
 * @returns {Promise<Array>} Summary and device-list responses.
 */
function loadDashboard() {
  return Promise.all([getSummary(), getDevices()])
}

/**
 * Dashboard built from docs/design/dashboard-mockup.png with sample data.
 * @returns {JSX.Element} The page.
 */
export default function DashboardPage() {
  const { data, error } = useSampleData(loadDashboard)
  if (error) return <Text role="alert">Unable to load dashboard.</Text>
  if (!data) return <Text>Loading dashboard...</Text>
  const [summary, { devices }] = data
  const referenceTime = sampleScreen.generated_at
  const cards = [
    { state: 'active', icon: Monitor },
    { state: 'idle', icon: Clock },
    { state: 'unused', icon: Power },
  ]

  return (
    <Stack gap="md">
      <Header title="Welcome to EcoWattSense!"
        subtitle="Here's an overview of your computers and energy usage."
        generatedAt={referenceTime} userName={sampleScreen.user_name} />
      <SimpleGrid cols={4} spacing="md">
        {cards.map(({ state, icon }) => (
          <StatCard key={state} state={state} icon={icon}
            count={summary.counts[state]} total={summary.counts.total}
            label={stateDisplay[state].label} caption={stateDisplay[state].caption} />
        ))}
        <EnergyCard energy_uj={summary.energy_uj} cost_gbp={summary.cost_gbp}
          co2_kg={summary.co2_kg} price_gbp_per_kwh={summary.price_gbp_per_kwh}
          co2_kg_per_kwh={summary.co2_kg_per_kwh} />
      </SimpleGrid>
      <Grid gutter="md">
        <Grid.Col span={8}><EnergyChart points={summary.series} /></Grid.Col>
        <Grid.Col span={4}><StatusDonut summary={summary} /></Grid.Col>
      </Grid>
      <ComputerTable computers={devices} limit={5} showAllLink referenceTime={referenceTime} />
    </Stack>
  )
}
