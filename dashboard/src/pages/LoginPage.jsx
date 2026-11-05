// Login page (#login). Opened by App.jsx.
import { Stack, Text } from '@mantine/core'
import Header from '../components/Header.jsx'

/**
 * Login page. It is empty for now: the form and the route guard come in EWS_WEB-33.
 *
 * @param {object} props
 * @param {object} props.data - The sample data, for the header (date and user name).
 * @returns {JSX.Element} The page.
 */
export default function LoginPage({ data }) {
  return (
    <Stack gap="md">
      <Header
        title="Login"
        subtitle="Log in to see your computers."
        generatedAt={data.generatedAt}
        userName={data.user.name}
      />
      <Text>Coming in this sprint</Text>
    </Stack>
  )
}
