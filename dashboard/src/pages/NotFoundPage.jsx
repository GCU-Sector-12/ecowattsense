// "Page not found" page. App.jsx shows it for every address that is not a page.
import { Anchor, Stack } from '@mantine/core'
import Header from '../components/Header.jsx'

/**
 * Page for an unknown address, with a link back to the Dashboard.
 *
 * @param {object} props
 * @param {object} props.data - The sample data, for the header (date and user name).
 * @returns {JSX.Element} The page.
 */
export default function NotFoundPage({ data }) {
  return (
    <Stack gap="md">
      <Header
        title="Page not found"
        subtitle="This address does not exist."
        generatedAt={data.generatedAt}
        userName={data.user.name}
      />
      <Anchor href="#dashboard">Back to the Dashboard</Anchor>
    </Stack>
  )
}
