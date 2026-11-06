// Full computer-list page, rendered by App.jsx for #computers.
// Loads devices through the shared provider hook rather than duplicating its lifecycle.
import useSampleData from '../useSampleData.js'
import { Stack, Text } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'
import ComputerTable from '../components/ComputerTable.jsx'
import { getDevices } from '../data/index.js'

/**
 * Full computer list, rather than the five-row dashboard preview.
 * @returns {JSX.Element} The page.
 */
export default function ComputersPage() {
  const { data, error } = useSampleData(getDevices)
  const devices = data?.devices ?? []

  return (
    <Stack gap="md">
      <Header title="Computers"
        subtitle={`All ${devices.length} monitored computers and their current state.`}
        generatedAt={sampleScreen.generated_at} userName={sampleScreen.user_name} />
      {error && <Text role="alert">Unable to load computers.</Text>}
      <ComputerTable title="All Computers" computers={devices}
        referenceTime={sampleScreen.generated_at} />
    </Stack>
  )
}
