import { Grid, SimpleGrid, Stack } from '@mantine/core'
import { Monitor, Clock, Power } from 'lucide-react'
import Header from '../components/Header.jsx'
import StatCard from '../components/StatCard.jsx'
import EnergyCard from '../components/EnergyCard.jsx'
import EnergyChart from '../components/EnergyChart.jsx'
import StatusDonut from '../components/StatusDonut.jsx'
import ComputerTable from '../components/ComputerTable.jsx'

// Dashboard page built from docs/design/dashboard-mockup.png with sample data.
export default function DashboardPage({ data }) {
  const { summary } = data

  return (
    <Stack gap="md">
      <Header
        title="Welcome to EcoWattSense!"
        subtitle="Here's an overview of your computers and energy usage."
        generatedAt={data.generatedAt}
        userName={data.user.name}
      />

      <SimpleGrid cols={4} spacing="md">
        <StatCard state="active" icon={Monitor} stat={summary.active} />
        <StatCard state="idle" icon={Clock} stat={summary.idle} />
        <StatCard state="unused" icon={Power} stat={summary.unused} />
        <EnergyCard kwh={summary.energyTodayKWh} cost={summary.estimatedCostGBP} />
      </SimpleGrid>

      <Grid gutter="md">
        <Grid.Col span={8}>
          <EnergyChart points={data.energyToday.points} />
        </Grid.Col>
        <Grid.Col span={4}>
          <StatusDonut summary={summary} />
        </Grid.Col>
      </Grid>

      <ComputerTable computers={data.computers.slice(0, 5)} />
    </Stack>
  )
}
