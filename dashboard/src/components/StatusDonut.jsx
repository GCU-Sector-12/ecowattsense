// Renders a state label, count and percentage of all devices.
import { Box, Card, Center, Group, Stack, Text } from '@mantine/core'
import { DonutChart } from '@mantine/charts'
import StatusDot from './StatusDot.jsx'

import { stateLabels } from '../data/ui.js'

const STATES = ['active', 'idle', 'unused', 'no_data']

// One row of the legend: dot, word, count and share in percent.
/**
 * Renders a state label, count and percentage of all devices.
 * @param {object} props - State, count object and total device count.
 * @returns {JSX.Element} Rendered content.
 */
function LegendRow({ state, stat, total }) {
  return (
    <Group justify="space-between" wrap="nowrap" gap="md">
      <Group gap="xs" wrap="nowrap">
        <StatusDot state={state} size={16} />
        <Text fz="sm">{stateLabels[state]}</Text>
      </Group>
      <Group gap="xs" wrap="nowrap">
        <Text fz="sm" fw={700}>
          {stat.count}
        </Text>
        <Text fz="sm" c="var(--ews-text-muted)" style={{ whiteSpace: 'nowrap' }}>
          ({Math.round((stat.count / (total || 1)) * 100)}%)
        </Text>
      </Group>
    </Group>
  )
}

// "Computer Status" card: donut of active / idle / unused with the total in the centre.
/**
 * Renders the distribution of all four device states.
 * @param {object} props - Summary containing contract counts.
 * @returns {JSX.Element} Rendered content.
 */
export default function StatusDonut({ summary }) {
  const data = STATES.map((state) => ({
    name: stateLabels[state],
    value: summary.counts[state],
    color: state === 'no_data' ? 'gray' : state,
  }))

  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" h="100%">
      <Text fz="lg" fw={700} mb="md">
        Computer Status
      </Text>
      <Group justify="space-between" align="center" wrap="nowrap" gap="sm" mt="xs">
        <Box pos="relative">
          <DonutChart
            data={data}
            size={160}
            thickness={30}
            strokeWidth={2}
            withTooltip={false}
            startAngle={90}
            endAngle={-270}
          />
          <Center pos="absolute" inset={0}>
            <Stack gap={0} align="center">
              <Text fz={28} fw={700} lh={1.1}>
                {summary.counts.total}
              </Text>
              <Text fz="sm" c="var(--ews-text-muted)">
                Computers
              </Text>
            </Stack>
          </Center>
        </Box>

        <Stack gap="sm" style={{ flex: 1 }}>
          {STATES.map((state) => (
            <LegendRow key={state} state={state} stat={{ count: summary.counts[state] }} total={summary.counts.total} />
          ))}
        </Stack>
      </Group>
    </Card>
  )
}
