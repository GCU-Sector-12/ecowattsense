import { Card, Progress, Stack, Text } from '@mantine/core'

// Stat card for one state (active, idle, unused). Tinted background, icon and
// progress bar use the status colour. `state` is also a theme colour name.
export default function StatCard({ state, icon: Icon, stat }) {
  return (
    <Card radius="lg" padding="lg" bg={`var(--ews-${state}-bg)`}>
      <Stack gap="xs">
        <Icon size={48} color={`var(--ews-${state})`} strokeWidth={2} />
        <Text fz={36} fw={700} lh={1.1} mt="xs">
          {stat.count}
        </Text>
        <div>
          <Text fz="md" fw={500} lh={1.3}>
            {stat.label}
          </Text>
          <Text fz="sm" c="var(--ews-text-muted)" lh={1.3}>
            {stat.caption}
          </Text>
        </div>
        <Progress
          value={stat.share * 100}
          color={state}
          size={10}
          radius="xl"
          mt="xs"
          styles={{ root: { backgroundColor: 'var(--ews-track)' } }}
          aria-label={`${stat.label}: ${Math.round(stat.share * 100)}%`}
        />
      </Stack>
    </Card>
  )
}
