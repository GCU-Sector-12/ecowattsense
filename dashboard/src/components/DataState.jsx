// Shared request feedback for data pages. Pass loading, error, empty and retry.
// Neutral tokens keep request failures separate from computer state colours.
import { Button, Card, Skeleton, Stack, Text } from '@mantine/core'

/**
 * Shows an accessible loading, error or empty card.
 * @param {object} props - Request state and retry callback.
 * @param {boolean} [props.loading] - A request has no result yet.
 * @param {Error|null} [props.error] - Failure to explain without exposing server details.
 * @param {boolean} [props.empty] - A successful response has no items.
 * @param {Function} [props.onRetry] - Starts another request.
 * @param {string} [props.label] - Name of this data region.
 * @returns {JSX.Element|null} Feedback card, or nothing for a populated result.
 */
export default function DataState({ loading, error, empty, onRetry, label = 'Data' }) {
  if (error) return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" role="alert">
      <Stack gap="sm">
        <Text>Could not load data. Try again.</Text>
        <Button onClick={onRetry} variant="default">Retry</Button>
      </Stack>
    </Card>
  )
  if (loading) return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" role="status" aria-label={`Loading ${label}`}>
      <Stack gap="sm" aria-hidden="true">
        <Skeleton height={24} width="45%" />
        <Skeleton height={80} />
        <Skeleton height={16} width="75%" />
      </Stack>
    </Card>
  )
  if (empty) return (
    <Card radius="lg" padding="lg" bg="var(--ews-card)" role="status" aria-label={label}>
      <Text>No data</Text>
    </Card>
  )
  return null
}
