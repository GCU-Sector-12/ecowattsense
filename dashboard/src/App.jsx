// The app shell: the sidebar on the left and the page for the current address on the right.
import { AppShell } from '@mantine/core'
import { useEffect, useState } from 'react'
import data from './mock/dashboard-sample-data.json'
import useHash, { resolveRoute } from './useHash.js'
import Sidebar from './components/Sidebar.jsx'
import sidebarClasses from './components/Sidebar.module.css'
import DashboardPage from './pages/DashboardPage.jsx'
import ComputersPage from './pages/ComputersPage.jsx'
import EnergyPage from './pages/EnergyPage.jsx'
import HistoryPage from './pages/HistoryPage.jsx'
import LoginPage from './pages/LoginPage.jsx'
import NotFoundPage from './pages/NotFoundPage.jsx'
import SettingsPage from './pages/SettingsPage.jsx'

const DEFAULT_SIDEBAR = { width: 300, collapsed: false }
const NARROW_WIDTH = 72
const MIN_WIDTH = 220
const MAX_WIDTH = 360
const SIDEBAR_STORAGE_KEY = 'ews-sidebar'

/** Restores a valid saved sidebar preference when browser storage is available. */
function readSidebar() {
  try {
    const saved = JSON.parse(localStorage.getItem(SIDEBAR_STORAGE_KEY))
    if (typeof saved?.collapsed === 'boolean' && Number.isFinite(saved.width)) {
      return { width: Math.min(MAX_WIDTH, Math.max(MIN_WIDTH, saved.width)), collapsed: saved.collapsed }
    }
  } catch {
    // The sidebar can still be used when browser storage is unavailable.
  }
  return DEFAULT_SIDEBAR
}

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
  const [sidebar, setSidebar] = useState(readSidebar)
  const [resizing, setResizing] = useState(false)
  const sidebarWidth = sidebar.collapsed ? NARROW_WIDTH : sidebar.width

  useEffect(() => {
    if (resizing) return
    try {
      localStorage.setItem(SIDEBAR_STORAGE_KEY, JSON.stringify(sidebar))
    } catch {
      // The preference lasts for this visit when browser storage is unavailable.
    }
  }, [sidebar, resizing])

  function toggleSidebar() {
    setSidebar((current) => ({ ...current, collapsed: !current.collapsed }))
  }

  function resizeSidebar(width, finished) {
    setSidebar((current) => {
      if (finished && width < MIN_WIDTH) return { ...current, collapsed: true }
      return { width: Math.min(MAX_WIDTH, Math.max(MIN_WIDTH, width)), collapsed: false }
    })
  }

  return (
    <AppShell navbar={{ width: sidebarWidth, breakpoint: 0 }} padding={32} transitionDuration={resizing ? 0 : 150}>
      <AppShell.Navbar className={sidebarClasses.navbar} bg="var(--ews-green-900)" withBorder={false} data-width={sidebarWidth}>
        <Sidebar
          items={data.navigation}
          activeKey={route.menuKey}
          collapsed={sidebar.collapsed}
          onToggle={toggleSidebar}
          onResize={resizeSidebar}
          onResizingChange={setResizing}
        />
      </AppShell.Navbar>

      <AppShell.Main bg="var(--ews-page)">
        <Page data={data} deviceId={route.deviceId} />
      </AppShell.Main>
    </AppShell>
  )
}
