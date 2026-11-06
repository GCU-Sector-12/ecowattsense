// Computer history page (#computers/<device_id>). Opened from the Computers list by App.jsx.
import { Stack, Text } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'

/**
 * History of one computer. It is empty for now: the timeline and totals come in EWS_WEB-30.
 *
 * @param {object} props
 * @param {string} props.deviceId - The computer from the address, for example "abc" in #computers/abc.
 * @returns {JSX.Element} The page.
 */
export default function HistoryPage({ deviceId }) {
  return (
    <Stack gap="md">
      <Header
        title="Computer history"
        subtitle={`Computer ${deviceId}`}
        generatedAt={sampleScreen.generated_at}
        userName={sampleScreen.user_name}
      />
      <Text>Coming in this sprint</Text>
    </Stack>
  )
}
