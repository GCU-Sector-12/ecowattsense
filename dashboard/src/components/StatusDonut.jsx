// Renders a state label, count and percentage of all devices.
import { Box, Card, Center, Group, Stack, Text } from '@mantine/core'
import { DonutChart } from '@mantine/charts'
import StatusDot from './StatusDot.jsx'
import { stateColors, stateLabels } from '../data/ui.js'
import { statusPercentages } from './statusPercentages.js'

const STATES = ['active', 'idle', 'unused', 'no_data']

// Every state keeps its word and count, including a zero-count state.
/**
 * Renders one legend row: dot, state word, count and share in percent.
 * @param {object} props - State key, device count and whole percent for that state.
 * @returns {JSX.Element} Rendered content.
 */
function LegendRow({ state, count, percent }) {
  return (
    <Group component="li" justify="space-between" wrap="nowrap" gap="md">
      <Group gap="xs" wrap="nowrap">
        <StatusDot state={state} size={16} />
        <Text fz="sm">{stateLabels[state]}</Text>
      </Group>
      <Text fz="sm" style={{ whiteSpace: 'nowrap' }}>
        <Text span fw={700}>{count}</Text>{' '}
        <Text span c="var(--ews-text-muted)">({percent}%)</Text>
      </Text>
    </Group>
  )
}

// The four-state chart and legend both read the same summary counts.
/**
 * Renders the distribution of all four device states.
 * @param {object} props - Summary containing contract counts.
 * @returns {JSX.Element} Rendered content.
 */
export default function StatusDonut({ summary }) {
  const { counts } = summary
  const percentages = statusPercentages(STATES.map((state) => counts[state]), counts.total)
  const data = STATES.map((state) => ({
    name: stateLabels[state],
    value: counts[state],
    color: stateColors[state],
  }))

  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" h="100%">
      <Text fz="lg" fw={700} mb="md">
        Computer Status
      </Text>
      <Group justify="center" align="center" gap="sm" mt="xs">
        <Box pos="relative" role="img" aria-label={`${counts.total} computers in total`}>
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
                {counts.total}
              </Text>
              <Text fz="sm" c="var(--ews-text-muted)">
                Computers
              </Text>
            </Stack>
          </Center>
        </Box>

        <Stack component="ul" aria-label="Computer status breakdown" gap="sm"
          m={0} p={0} style={{ flex: '1 1 120px', listStyle: 'none' }}>
          {STATES.map((state, index) => (
            <LegendRow key={state} state={state} count={counts[state]} percent={percentages[index]} />
          ))}
        </Stack>
      </Group>
    </Card>
  )
}
