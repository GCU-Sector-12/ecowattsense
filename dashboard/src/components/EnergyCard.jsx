// Displays sample energy in kWh and the supplied estimated cost.
import { Card, Group, Stack, Text } from '@mantine/core'
import { Info, Leaf } from 'lucide-react'
import { ujToKwh, formatGbp } from '../data/units.js'

// Summary card: estimated energy and cost for today. Grey background (card-alt).
/**
 * Displays sample energy in kWh and the supplied estimated cost.
 * @param {object} props - Contract energy_uj and cost_gbp values.
 * @returns {JSX.Element} Rendered content.
 */
export default function EnergyCard({ energy_uj, cost_gbp }) {
  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card-alt)">
      <Stack gap="md" justify="space-between" h="100%">
        <Group gap="sm" wrap="nowrap" align="center">
          <Leaf size={44} color="var(--ews-icon-brand, var(--ews-green-700))" strokeWidth={1.75} />
          <div>
            <Text fz="sm" fw={500} lh={1.3} style={{ whiteSpace: 'nowrap' }}>
              Estimated Energy Usage
            </Text>
            <Text fz={36} fw={700} lh={1.1}>
              {ujToKwh(energy_uj).toFixed(1)} kWh
            </Text>
            <Text fz="sm" c="var(--ews-text-muted)">
              Today
            </Text>
          </div>
        </Group>

        <Group justify="space-between" wrap="nowrap">
          <Text fz="sm">
            Estimated Cost:{' '}
            <Text span fw={700}>
              {formatGbp(cost_gbp)}
            </Text>
          </Text>
          <Info size={20} color="var(--ews-text-muted)" aria-label="How the estimate is made" />
        </Group>
      </Stack>
    </Card>
  )
}
