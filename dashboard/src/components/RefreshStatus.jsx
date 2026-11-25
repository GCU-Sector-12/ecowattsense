// Refresh status line used by Dashboard, Computers, History and Energy pages.
import { Text } from '@mantine/core'

/**
 * Shows the last successful update, or a quiet warning when the latest refresh failed.
 * @param {{lastUpdatedAt: Date|null, refreshError: Error|null}} props - Refresh status from useSampleData.
 * @returns {JSX.Element|null} A small status line after the page heading.
 */
export default function RefreshStatus({ lastUpdatedAt, refreshError }) {
  if (!lastUpdatedAt) return null
  const time = lastUpdatedAt.toLocaleTimeString('en-GB', {
    hour: '2-digit', minute: '2-digit', second: '2-digit',
  })
  const message = refreshError ? `Could not update. Last update ${time}.` : `Updated ${time}`
  return <Text size="xs" c="var(--ews-text-muted)" aria-live="polite">{message}</Text>
}
