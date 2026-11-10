// Displays energy in kWh, the estimated cost and CO2, and the tariff used for the estimate.
import { ActionIcon, Card, Group, Stack, Text, Tooltip } from '@mantine/core'
import { Info, Leaf } from 'lucide-react'
import { ujToKwh, formatGbp } from '../data/units.js'

// Summary card: API energy, cost, CO2 and the tariff used for estimates.
/**
 * Displays energy in kWh, the estimated cost and CO2, and explains the tariff in a tooltip.
 * @param {object} props - Contract energy_uj, cost_gbp and co2_kg values, and the tariff
 *   price_gbp_per_kwh and co2_kg_per_kwh used for the estimate.
 * @returns {JSX.Element} Rendered content.
 */
export default function EnergyCard({
  energy_uj, cost_gbp, co2_kg, price_gbp_per_kwh, co2_kg_per_kwh,
}) {
  const estimate = `Estimate. Price ${price_gbp_per_kwh} GBP per kWh, ${co2_kg_per_kwh} kg CO2 per kWh`
  return (
    <Card radius="lg" padding="lg" bg="var(--ews-card-alt)">
      <Stack gap="md" justify="space-between" h="100%">
        <Group gap="sm" wrap="nowrap" align="center">
          <Leaf size={44} color="var(--ews-icon-brand, var(--ews-green-700))" strokeWidth={1.75} />
          <div>
            <Text fz="sm" fw={500} lh={1.3}>
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
          <Tooltip label={estimate} events={{ hover: true, focus: true, touch: true }}
            multiline w={280} withArrow>
            <ActionIcon variant="subtle" color="gray" aria-label="How the estimate is made">
              <Info size={20} />
            </ActionIcon>
          </Tooltip>
        </Group>
        <Text fz="sm">
          Estimated CO₂:{' '}
          <Text span fw={700}>{co2_kg.toFixed(2)} kg</Text>
        </Text>
      </Stack>
    </Card>
  )
}
