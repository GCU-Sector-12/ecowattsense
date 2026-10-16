import { Box } from '@mantine/core'

// Small filled circle in the status colour. Always put a word next to it.
export default function StatusDot({ state, size = 12 }) {
  return (
    <Box
      component="span"
      w={size}
      h={size}
      bg={`var(--ews-${state})`}
      style={{ borderRadius: '50%', display: 'inline-block', flexShrink: 0 }}
      aria-hidden="true"
    />
  )
}
