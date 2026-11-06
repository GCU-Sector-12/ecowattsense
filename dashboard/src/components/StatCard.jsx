// Shows a state count and its proportion of all devices.
import { Card, Progress, Stack, Text } from '@mantine/core'

// Stat card for one state (active, idle, unused). Tinted background, icon and
// progress bar use the status colour. `state` is also a theme colour name.
/**
 * Shows a state count and its proportion of all devices.
 * @param {object} props - State, icon, count, total, label and caption.
 * @returns {JSX.Element} Rendered content.
 */
export default function StatCard({ state, icon: Icon, count, total, label, caption }) {
  return (
    <Card radius="lg" padding="lg" bg={`var(--ews-${state}-bg)`}>
      <Stack gap="xs">
        <Icon size={48} color={`var(--ews-${state})`} strokeWidth={2} />
        <Text fz={36} fw={700} lh={1.1} mt="xs">
          {count}
        </Text>
        <div>
          <Text fz="md" fw={500} lh={1.3}>
            {label}
          </Text>
          <Text fz="sm" c="var(--ews-text-muted)" lh={1.3}>
            {caption}
          </Text>
        </div>
        <Progress
          value={(count / (total || 1)) * 100}
          color={state}
          size={10}
          radius="xl"
          mt="xs"
          styles={{ root: { backgroundColor: 'var(--ews-track)' } }}
          aria-label={`${label}: ${Math.round((count / (total || 1)) * 100)}%`}
        />
      </Stack>
    </Card>
  )
}
