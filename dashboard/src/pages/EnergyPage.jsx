// Energy Usage page (#energy). Opened from the sidebar by App.jsx.
import { Stack, Text } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'

/**
 * Energy Usage page. It is empty for now: the energy table and chart come in EWS_WEB-31.
 *
 * @returns {JSX.Element} The page.
 */
export default function EnergyPage() {
  return (
    <Stack gap="md">
      <Header
        title="Energy Usage"
        subtitle="Energy, cost and CO2 for each day."
        generatedAt={sampleScreen.generated_at}
        userName={sampleScreen.user_name}
      />
      <Text>Coming in this sprint</Text>
    </Stack>
  )
}
