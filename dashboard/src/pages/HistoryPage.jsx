// Computer history page (#computers/<device_id>). Opened from the Computers list by App.jsx.
import { Stack, Text } from '@mantine/core'
import Header from '../components/Header.jsx'

/**
 * History of one computer. It is empty for now: the timeline and totals come in EWS_WEB-30.
 *
 * @param {object} props
 * @param {object} props.data - The sample data, for the header (date and user name).
 * @param {string} props.deviceId - The computer from the address, for example "abc" in #computers/abc.
 * @returns {JSX.Element} The page.
 */
export default function HistoryPage({ data, deviceId }) {
  return (
    <Stack gap="md">
      <Header
        title="Computer history"
        subtitle={`Computer ${deviceId}`}
        generatedAt={data.generatedAt}
        userName={data.user.name}
      />
      <Text>Coming in this sprint</Text>
    </Stack>
  )
}
