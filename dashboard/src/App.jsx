// App entry used by main.jsx. Mount <App /> to show hash-based pages and navigation.
import { AppShell } from '@mantine/core'
import useHash, { resolveRoute } from './useHash.js'
import Sidebar from './components/Sidebar.jsx'
import DashboardPage from './pages/DashboardPage.jsx'
import ComputersPage from './pages/ComputersPage.jsx'
import EnergyPage from './pages/EnergyPage.jsx'
import HistoryPage from './pages/HistoryPage.jsx'
import LoginPage from './pages/LoginPage.jsx'
import NotFoundPage from './pages/NotFoundPage.jsx'
import SettingsPage from './pages/SettingsPage.jsx'
import { navigation } from './data/ui.js'

const SIDEBAR_WIDTH = 300
const PAGES = {
  dashboard: DashboardPage,
  computers: ComputersPage,
  history: HistoryPage,
  energy: EnergyPage,
  settings: SettingsPage,
  login: LoginPage,
  'not-found': NotFoundPage,
}

/**
 * Shows the sidebar and the page selected by the browser hash.
 * @returns {JSX.Element} The whole app.
 */
export default function App() {
  const route = resolveRoute(useHash())
  const Page = PAGES[route.page]
  return (
    <AppShell navbar={{ width: SIDEBAR_WIDTH, breakpoint: 0 }} padding={32}>
      <AppShell.Navbar bg="var(--ews-green-900)" withBorder={false}>
        <Sidebar items={navigation} activeKey={route.menuKey} />
      </AppShell.Navbar>
      <AppShell.Main bg="var(--ews-page)">
        <Page deviceId={route.deviceId} />
      </AppShell.Main>
    </AppShell>
  )
}
