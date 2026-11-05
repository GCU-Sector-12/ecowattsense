import { useEffect, useState } from 'react'
import { Stack } from '@mantine/core'
import Header from '../components/Header.jsx'
import ComputerTable from '../components/ComputerTable.jsx'
import { getDevices } from '../data/index.js'

export default function ComputersPage() {
  const [devices, setDevices] = useState([])
  useEffect(() => {
    let mounted = true
    getDevices().then((response) => { if (mounted) setDevices(response.devices) })
    return () => { mounted = false }
  }, [])

  return (
    <Stack gap="md">
      <Header title="Computers"
        subtitle={`All ${devices.length} monitored computers and their current state.`}
        generatedAt="2025-03-10T14:32:00Z" userName="Admin" />
      <ComputerTable title="All Computers" computers={devices}
        referenceTime="2025-03-10T14:32:00Z" />
    </Stack>
  )
}
