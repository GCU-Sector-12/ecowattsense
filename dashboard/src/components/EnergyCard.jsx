import { Card, Group, Stack, Text } from '@mantine/core'
import { Info, Leaf } from 'lucide-react'

// Summary card: estimated energy and cost for today. Grey background (card-alt).
export default function EnergyCard({ kwh, cost }) {
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
              {kwh} kWh
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
              £{cost.toFixed(2)}
            </Text>
          </Text>
          <Info size={20} color="var(--ews-text-muted)" aria-label="How the estimate is made" />
        </Group>
      </Stack>
    </Card>
  )
}
