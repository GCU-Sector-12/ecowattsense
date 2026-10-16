import { Stack } from '@mantine/core'
import Header from '../components/Header.jsx'
import ComputerTable from '../components/ComputerTable.jsx'

// Computers page: the full list of computers (all rows, not only the first five).
export default function ComputersPage({ data }) {
  const total = data.computers.length

  return (
    <Stack gap="md">
      <Header
        title="Computers"
        subtitle={`All ${total} monitored computers and their current state.`}
        generatedAt={data.generatedAt}
        userName={data.user.name}
      />
      <ComputerTable title="All Computers" computers={data.computers} />
    </Stack>
  )
}
