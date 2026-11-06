// Login page (#login). Opened by App.jsx.
import { Stack, Text } from '@mantine/core'
import Header from '../components/Header.jsx'
import { sampleScreen } from '../data/ui.js'

/**
 * Login page. It is empty for now: the form and the route guard come in EWS_WEB-33.
 *
 * @returns {JSX.Element} The page.
 */
export default function LoginPage() {
  return (
    <Stack gap="md">
      <Header
        title="Login"
        subtitle="Log in to see your computers."
        generatedAt={sampleScreen.generated_at}
        userName={sampleScreen.user_name}
      />
      <Text>Coming in this sprint</Text>
    </Stack>
  )
}
