import { AppShell } from '@mantine/core'
import data from './mock/dashboard-sample-data.json'
import useHash from './useHash.js'
import Sidebar from './components/Sidebar.jsx'
import DashboardPage from './pages/DashboardPage.jsx'
import ComputersPage from './pages/ComputersPage.jsx'
import SettingsPage from './pages/SettingsPage.jsx'

const SIDEBAR_WIDTH = 300

// Page for each menu key. A key without a page yet (Energy Usage, Reports) shows the dashboard.
const PAGES = {
  dashboard: DashboardPage,
  computers: ComputersPage,
  settings: SettingsPage,
}

export default function App() {
  const hash = useHash()
  const activeKey = hash in PAGES ? hash : 'dashboard'
  const Page = PAGES[activeKey]

  return (
    <AppShell navbar={{ width: SIDEBAR_WIDTH, breakpoint: 0 }} padding={32}>
      <AppShell.Navbar bg="var(--ews-green-900)" withBorder={false}>
        <Sidebar items={data.navigation} activeKey={activeKey} />
      </AppShell.Navbar>

      <AppShell.Main bg="var(--ews-page)">
        <Page data={data} />
      </AppShell.Main>
    </AppShell>
  )
}
