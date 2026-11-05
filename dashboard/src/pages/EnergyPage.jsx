// Energy Usage page (#energy). Opened from the sidebar by App.jsx.
import { Stack, Text } from '@mantine/core'
import Header from '../components/Header.jsx'

/**
 * Energy Usage page. It is empty for now: the energy table and chart come in EWS_WEB-31.
 *
 * @param {object} props
 * @param {object} props.data - The sample data, for the header (date and user name).
 * @returns {JSX.Element} The page.
 */
export default function EnergyPage({ data }) {
  return (
    <Stack gap="md">
      <Header
        title="Energy Usage"
        subtitle="Energy, cost and CO2 for each day."
        generatedAt={data.generatedAt}
        userName={data.user.name}
      />
      <Text>Coming in this sprint</Text>
    </Stack>
  )
}
