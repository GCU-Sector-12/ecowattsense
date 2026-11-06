// "Page not found" page. App.jsx shows it for every address that is not a page.
import { Anchor, Stack } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'

/**
 * Page for an unknown address, with a link back to the Dashboard.
 *
 * @returns {JSX.Element} The page.
 */
export default function NotFoundPage() {
  return (
    <Stack gap="md">
      <Header
        title="Page not found"
        subtitle="This address does not exist."
        generatedAt={sampleScreen.generated_at}
        userName={sampleScreen.user_name}
      />
      <Anchor href="#dashboard">Back to the Dashboard</Anchor>
    </Stack>
  )
}
