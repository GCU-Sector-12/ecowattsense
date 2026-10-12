import './StateBadge.css'

const LABELS = {
  active: 'Active',
  idle: 'Idle',
  unused: 'Unused',
}

// The state is always shown with a word, never with colour only (ui-style-guide.md).
export default function StateBadge({ state }) {
  return <span className={`badge badge-${state}`}>{LABELS[state]}</span>
}
