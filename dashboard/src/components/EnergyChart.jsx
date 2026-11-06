import { Card, Stack, Text, useMantineTheme } from '@mantine/core'
import { AreaChart } from '@mantine/charts'
import { formatUkTime, toEnergyChartData } from './energyChartData.js'

// Chart ticks follow UK wall-clock four-hour boundaries, not fixed UTC offsets.
function getFourHourTicks(data) {
  const ticks = data.filter(({ timestamp }) => {
    const parts = new Intl.DateTimeFormat('en-GB', {
      timeZone: 'Europe/London', hour: '2-digit', minute: '2-digit', hourCycle: 'h23',
    }).formatToParts(new Date(timestamp))
    const hour = Number(parts.find((part) => part.type === 'hour')?.value)
    const minute = Number(parts.find((part) => part.type === 'minute')?.value)
    return minute === 0 && hour % 4 === 0
  }).map(({ timestamp }) => timestamp)

  // Short or partial series may not reach a wall-clock boundary.
  return ticks.length ? ticks : [data[0].timestamp]
}

function getPowerAxis(data) {
  const peak = Math.max(...data.map(({ power_kw }) => power_kw), 0)
  const maximum = peak === 0 ? 0.5 : Math.max(0.5, Math.ceil(peak * 2) / 2)
  return {
    domain: [0, maximum],
    ticks: Array.from({ length: 5 }, (_, index) => Number((maximum * index / 4).toFixed(2))),
    tickMargin: 8,
    width: 42,
  }
}

function formatKw(value) {
  return Number(value).toFixed(1)
}

function EnergyTooltip({ active, payload, label }) {
  if (!active || !payload?.length) return null
  const point = payload[0].payload
  return (
    <Card padding="xs" radius="sm" shadow="sm" withBorder>
      <Stack gap={2}>
        <Text size="xs" fw={600}>{formatUkTime(label)}</Text>
        <Text size="xs">Power: {formatKw(point.power_kw)} kW</Text>
        <Text size="xs">Energy: {point.energy_kwh.toFixed(3)} kWh</Text>
      </Stack>
    </Card>
  )
}

// "Energy Usage (Today)" card with power and energy detail for each bucket.
export default function EnergyChart({ points }) {
  const theme = useMantineTheme()
  const data = toEnergyChartData(points)
  const xTicks = data.length ? getFourHourTicks(data) : []
  const yAxis = data.length ? getPowerAxis(data) : null

  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" h="100%">
      <Text fz="lg" fw={700} mb="md">
        Energy Usage (Today)
      </Text>
      {data.length === 0 ? <Text>No data for this day</Text> : <AreaChart
        h={240}
        data={data}
        dataKey="timestamp"
        series={[{ name: 'power_kw', label: 'Power', color: theme.other.chartLine }]}
        curveType="linear"
        withDots={false}
        withGradient
        fillOpacity={0.35}
        strokeWidth={2}
        gridAxis="xy"
        strokeDasharray="4 4"
        gridColor="var(--ews-chart-grid)"
        tickLine="none"
        xAxisProps={{ ticks: xTicks, interval: 0, tickMargin: 8, tickFormatter: formatUkTime }}
        tooltipProps={{ content: EnergyTooltip, labelFormatter: formatUkTime }}
        yAxisProps={yAxis}
        yAxisLabel="Power (kW)"
        valueFormatter={formatKw}
      />}
    </Card>
  )
}
