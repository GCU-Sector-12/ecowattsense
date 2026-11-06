// The app shell: the sidebar on the left and the page for the current address on the right.
import { AppShell } from '@mantine/core'
import data from './mock/dashboard-sample-data.json'
import useHash, { resolveRoute } from './useHash.js'
import Sidebar from './components/Sidebar.jsx'
import DashboardPage from './pages/DashboardPage.jsx'
import ComputersPage from './pages/ComputersPage.jsx'
import EnergyPage from './pages/EnergyPage.jsx'
import HistoryPage from './pages/HistoryPage.jsx'
import LoginPage from './pages/LoginPage.jsx'
import NotFoundPage from './pages/NotFoundPage.jsx'
import SettingsPage from './pages/SettingsPage.jsx'

const SIDEBAR_WIDTH = 300

// The one list of every page: its name and its component. To add a page, add one line here.
// The names go to resolveRoute(). It is exported so a test can check them. Reports has no page,
// so its address is "not-found".
export const PAGES = {
  dashboard: DashboardPage,
  computers: ComputersPage,
  history: HistoryPage,
  energy: EnergyPage,
  settings: SettingsPage,
  login: LoginPage,
  'not-found': NotFoundPage,
}

/**
 * Shows the sidebar and the page that belongs to the address in the browser (the hash).
 *
 * @returns {JSX.Element} The whole app.
 */
export default function App() {
  const route = resolveRoute(useHash(), Object.keys(PAGES))
  const Page = PAGES[route.page]

  return (
    <AppShell navbar={{ width: SIDEBAR_WIDTH, breakpoint: 0 }} padding={32}>
      <AppShell.Navbar bg="var(--ews-green-900)" withBorder={false}>
        <Sidebar items={data.navigation} activeKey={route.menuKey} />
      </AppShell.Navbar>

      <AppShell.Main bg="var(--ews-page)">
        <Page data={data} deviceId={route.deviceId} />
      </AppShell.Main>
    </AppShell>
  )
}
