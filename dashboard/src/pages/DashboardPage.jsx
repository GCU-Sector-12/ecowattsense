import { useEffect, useState } from 'react'
import { Grid, SimpleGrid, Stack } from '@mantine/core'
import { Monitor, Clock, Power } from 'lucide-react'
import Header from '../components/Header.jsx'
import StatCard from '../components/StatCard.jsx'
import EnergyCard from '../components/EnergyCard.jsx'
import EnergyChart from '../components/EnergyChart.jsx'
import StatusDonut from '../components/StatusDonut.jsx'
import ComputerTable from '../components/ComputerTable.jsx'
import { getSummary, getDevices } from '../data/index.js'
import { sampleScreen, stateDisplay } from '../data/ui.js'

export default function DashboardPage() {
  const [summary, setSummary] = useState(null)
  const [devices, setDevices] = useState([])
  useEffect(() => {
    let mounted = true
    Promise.all([getSummary(), getDevices()]).then(([nextSummary, nextDevices]) => {
      if (mounted) {
        setSummary(nextSummary)
        setDevices(nextDevices.devices)
      }
    })
    return () => { mounted = false }
  }, [])

  if (!summary) return <div>Loading dashboard...</div>
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
        <EnergyCard energy_uj={summary.energy_uj} cost_gbp={summary.cost_gbp} />
      </SimpleGrid>
      <Grid gutter="md">
        <Grid.Col span={8}><EnergyChart points={summary.series} /></Grid.Col>
        <Grid.Col span={4}><StatusDonut summary={summary} /></Grid.Col>
      </Grid>
      <ComputerTable computers={devices} limit={5} showAllLink referenceTime={referenceTime} />
    </Stack>
  )
}
