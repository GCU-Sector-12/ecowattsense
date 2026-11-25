// Energy Usage page (/energy). Opened from the sidebar by App.jsx.
import { useEffect, useState } from 'react'
import { Alert, Card, Group, SegmentedControl, SimpleGrid, Stack, Table, Text, useMantineTheme } from '@mantine/core'
import { BarChart } from '@mantine/charts'
import Header from '../components/Header.jsx'
import { getSummary } from '../data/index.js'
import { chooseDisplayUnit } from '../data/units.js'
import sample from '../data/summary.json'
import { buildEnergyRows, formatEnergyCost, getEnergyTotals, getRangeDates } from '../data/energyUsage.js'

/**
 * Chooses the final date for the selected range in the active data mode.
 * @returns {string} Today in UK time for real data, or the latest sample date.
 */
function getEndDate() {
  if (import.meta.env.VITE_SUMMARY_DATA_SOURCE !== 'real') return sample.date
  return new Intl.DateTimeFormat('en-CA', {
    timeZone: 'Europe/London', year: 'numeric', month: '2-digit', day: '2-digit',
  }).format(new Date())
}

/**
 * Loads every daily summary required by the selected range.
 * @param {number} range - Number of days to load.
 * @returns {Promise<object[]>} Daily API summaries in date order.
 */
async function loadEnergyRows(range) {
  const dates = getRangeDates(range, getEndDate())
  const summaries = await Promise.all(dates.map((date) => getSummary(date)))
  return buildEnergyRows(summaries)
}

/**
 * Records a load failure and clears stale rows from the previous range.
 * @param {Error} cause - Error raised by the summary provider.
 * @param {(message: string) => void} setError - Page error state setter.
 * @param {(rows: object[]) => void} setRows - Daily rows state setter.
 * @returns {void}
 */
function handleLoadError(cause, setError, setRows) {
  setRows([])
  setError(cause.message || 'Please try again.')
}

/**
 * Renders range totals, a daily energy chart and a daily cost and CO2 table.
 * @returns {JSX.Element} Energy Usage page.
 */
export default function EnergyPage() {
  const theme = useMantineTheme()
  const [range, setRange] = useState('7')
  const [rows, setRows] = useState([])
  const [error, setError] = useState('')
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    setLoading(true)
    setError('')
    loadEnergyRows(Number(range))
      .then(setRows)
      .catch((cause) => handleLoadError(cause, setError, setRows))
      .finally(() => setLoading(false))
  }, [range])

  const totals = getEnergyTotals(rows)
  const display = chooseDisplayUnit(Math.max(0, ...rows.map(row => row.energy_kwh)))
  const totalDisplay = chooseDisplayUnit(totals.energy_kwh)
  const chartData = rows.map(row => ({ day: row.label, energy: row.energy_kwh * display.multiplier }))

  /**
   * Formats chart labels without rounding plotted data.
   * @param {number|string} value - Energy already scaled to the selected unit.
   * @returns {string} Energy label with the common chart unit. Does not throw.
   */
  function formatChartEnergy(value) {
    return `${Number(value).toFixed(display.unit === 'kWh' ? 3 : 2)} ${display.unit}`
  }

  return (
    <Stack gap="md">
      <Header
        title="Energy Usage"
        subtitle="Daily energy, cost and CO2 estimates."
        generatedAt={new Date().toISOString()}
        userName="Admin"
      />
      <Group justify="space-between" align="center">
        <Text fw={600}>Daily overview</Text>
        <SegmentedControl
          aria-label="Energy usage date range"
          value={range}
          onChange={setRange}
          data={[{ label: '7 days', value: '7' }, { label: '30 days', value: '30' }]}
        />
      </Group>
      {error && <Alert color="red" role="alert" title="Could not load energy usage">{error}</Alert>}
      <SimpleGrid cols={{ base: 1, sm: 3 }} spacing="md">
        <TotalCard label="Total energy" value={(totals.energy_kwh * totalDisplay.multiplier).toFixed(2) + ' ' + totalDisplay.unit} />
        <TotalCard label="Estimated cost" value={formatEnergyCost(totals.cost_gbp)} />
        <TotalCard label="Estimated CO₂" value={totals.co2_kg.toFixed(2) + ' kg'} />
      </SimpleGrid>
      <Card radius="lg" padding="lg" bg="var(--ews-card)">
        <Text fw={700} mb="md">Energy per day ({display.unit})</Text>
        {loading ? <Text>Loading energy usage…</Text> : <BarChart
          h={280}
          data={chartData}
          dataKey="day"
          yAxisLabel={`Energy (${display.unit})`}
          valueFormatter={formatChartEnergy}
          series={[{ name: 'energy', label: 'Energy', color: theme.other.chartLine }]}
          withLegend={false}
          gridAxis="y"
          tickLine="none"
          barProps={{ radius: 4 }}
          tooltipAnimationDuration={150}
        />}
      </Card>
      <Card radius="lg" padding="lg" bg="var(--ews-card)">
        <Text fw={700} mb="md">Daily breakdown</Text>
        <Table.ScrollContainer minWidth={560}>
          <Table striped={false} highlightOnHover>
            <Table.Thead>
              <Table.Tr>
                <Table.Th>Day</Table.Th>
                <Table.Th ta="right">Energy</Table.Th>
                <Table.Th ta="right">Cost</Table.Th>
                <Table.Th ta="right">CO₂ (kg)</Table.Th>
              </Table.Tr>
            </Table.Thead>
            <Table.Tbody>
              {rows.map((row) => <Table.Tr key={row.date}>
                <Table.Td>{row.label}</Table.Td>
                <Table.Td ta="right">{formatDailyEnergy(row.energy_kwh)}</Table.Td>
                <Table.Td ta="right">{formatEnergyCost(row.cost_gbp)}</Table.Td>
                <Table.Td ta="right">{row.co2_kg.toFixed(2)}</Table.Td>
              </Table.Tr>)}
            </Table.Tbody>
          </Table>
        </Table.ScrollContainer>
        {!loading && rows.length === 0 && <Text c="var(--ews-text-muted)">No data for this range.</Text>}
      </Card>
    </Stack>
  )
}

/**
 * Formats one daily reading independently, so small rows remain readable in a mixed table.
 * @param {number} energy - Daily energy in kWh.
 * @returns {string} Value and Wh or kWh unit, rounded only for display.
 * @throws {RangeError} If energy is not finite and nonnegative.
 */
function formatDailyEnergy(energy) {
  const display = chooseDisplayUnit(energy)
  return `${(energy * display.multiplier).toFixed(2)} ${display.unit}`
}

/**
 * Shows one aggregate value for the selected date range.
 * @param {{label: string, value: string}} props - Total label and formatted value.
 * @returns {JSX.Element} Summary card.
 */
function TotalCard({ label, value }) {
  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card-alt)">
      <Text size="sm" c="var(--ews-text-muted)">{label}</Text>
      <Text fw={700} size="xl">{value}</Text>
    </Card>
  )
}
