// Reusable device-state badge. Render <StateBadge state={state_name} /> in device views.
import { Badge } from '@mantine/core'
import StatusDot from './components/StatusDot.jsx'

import { stateLabels } from './data/ui.js'

// The state is always shown with a word, never with colour only (ui-style-guide.md).
// Mantine Badge in the "light" variant: tinted background, status colour text, dot on the left.
/**
 * Accessible state badge with a word and status dot.
 * @param {object} props - Badge options.
 * @param {string} props.state - API state name.
 * @returns {JSX.Element} Rendered content.
 */
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
      {stateLabels[state]}
    </Badge>
  )
}
