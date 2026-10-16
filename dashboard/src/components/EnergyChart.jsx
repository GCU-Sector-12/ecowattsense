import { Card, Text, useMantineTheme } from '@mantine/core'
import { AreaChart } from '@mantine/charts'

// Points are every 30 minutes, so a tick every 8 points shows 00:00, 04:00, 08:00...
const X_AXIS = { interval: 7, tickMargin: 8 }
const Y_AXIS = { domain: [0, 2], ticks: [0, 0.5, 1, 1.5, 2], tickMargin: 8, width: 36 }

// Ticks and tooltip values are shown with one decimal, as in the mockup (0.5, 1.0, 1.5, 2.0).
function formatKw(value) {
  return value.toFixed(1)
}

// "Energy Usage (Today)" card with an area chart of power in kW.
export default function EnergyChart({ points }) {
  const theme = useMantineTheme()

  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" h="100%">
      <Text fz="lg" fw={700} mb="md">
        Energy Usage (Today)
      </Text>
      <AreaChart
        h={240}
        data={points}
        dataKey="t"
        series={[{ name: 'kw', label: 'Power', color: theme.other.chartLine }]}
        curveType="linear"
        withDots={false}
        withGradient
        fillOpacity={0.35}
        strokeWidth={2}
        gridAxis="xy"
        strokeDasharray="4 4"
        gridColor="var(--ews-chart-grid)"
        tickLine="none"
        xAxisProps={X_AXIS}
        yAxisProps={Y_AXIS}
        yAxisLabel="Power (kW)"
        valueFormatter={formatKw}
      />
    </Card>
  )
}
