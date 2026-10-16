import { Badge } from '@mantine/core'
import StatusDot from './components/StatusDot.jsx'

const LABELS = {
  active: 'Active',
  idle: 'Idle',
  unused: 'Unused',
}

// The state is always shown with a word, never with colour only (ui-style-guide.md).
// Mantine Badge in the "light" variant: tinted background, status colour text, dot on the left.
export default function StateBadge({ state }) {
  return (
    <Badge
      variant="light"
      color={state}
      radius="md"
      size="lg"
      tt="none"
      fw={500}
      leftSection={<StatusDot state={state} size={8} />}
    >
      {LABELS[state]}
    </Badge>
  )
}
