// App entry used by main.jsx. Mount <App /> to show hash-based pages and navigation.
import { AppShell, Burger, Drawer, Group, Text } from '@mantine/core'
import { useDisclosure, useMediaQuery } from '@mantine/hooks'
import { useEffect, useState } from 'react'
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
import { navigation } from './data/ui.js'
import { getSampleSession, subscribeToSession } from './auth/sampleSession.js'

const DEFAULT_SIDEBAR = { width: 300, collapsed: false }
const NARROW_WIDTH = 72
const MIN_WIDTH = 220
const MAX_WIDTH = 360
const SIDEBAR_STORAGE_KEY = 'ews-sidebar'
const PHONE_WIDTH = 768
const DESKTOP_WIDTH = 1024

/**
 * Reads the raw sidebar preference from browser storage.
 * @returns {object|null} The parsed value, or null when storage is empty or unavailable.
 */
function loadSavedSidebar() {
  try {
    return JSON.parse(localStorage.getItem(SIDEBAR_STORAGE_KEY))
  } catch {
    // The sidebar can still be used when browser storage is unavailable.
    return null
  }
}

/**
 * Checks that a saved value has the shape the sidebar needs.
 * @param {object|null} saved - Value read from browser storage.
 * @returns {boolean} True when collapsed is a boolean and width is a finite number.
 */
function isSidebarPreference(saved) {
  return typeof saved?.collapsed === 'boolean' && Number.isFinite(saved?.width)
}

/**
 * Keeps a width inside the range allowed for the full sidebar.
 * @param {number} width - Requested width in px.
 * @returns {number} Width between MIN_WIDTH and MAX_WIDTH.
 */
function clampWidth(width) {
  return Math.min(MAX_WIDTH, Math.max(MIN_WIDTH, width))
}

/**
 * Restores the saved sidebar preference, or the default one when nothing valid is saved.
 * @returns {{width: number, collapsed: boolean}} Width of the full sidebar in px and the collapsed mode.
 */
function readSidebar() {
  const saved = loadSavedSidebar()
  if (!isSidebarPreference(saved)) return DEFAULT_SIDEBAR
  return { width: clampWidth(saved.width), collapsed: saved.collapsed }
}

/**
 * Saves the sidebar preference for the next visit, once a drag has finished,
 * so storage is not written on every pointer move.
 * @param {{width: number, collapsed: boolean}} sidebar - Current width and mode.
 * @param {boolean} resizing - True while the user drags the handle.
 * @returns {void}
 */
function saveSidebar(sidebar, resizing) {
  if (resizing) return
  try {
    localStorage.setItem(SIDEBAR_STORAGE_KEY, JSON.stringify(sidebar))
  } catch {
    // The preference lasts for this visit when browser storage is unavailable.
  }
}

/**
 * Gives the sidebar state with the collapsed mode switched.
 * @param {{width: number, collapsed: boolean}} current - State before the toggle.
 * @returns {{width: number, collapsed: boolean}} State after the toggle.
 */
function toggledSidebar(current) {
  return { ...current, collapsed: !current.collapsed }
}

/**
 * Gives the sidebar state after one drag step of the resize handle.
 * @param {{width: number, collapsed: boolean}} current - State before the step.
 * @param {number} width - Width from the handle in px.
 * @param {boolean} finished - True on the last step of the drag.
 * @returns {{width: number, collapsed: boolean}} Collapsed when a finished drag ends below
 *   MIN_WIDTH, otherwise the clamped width in full mode.
 */
function resizedSidebar(current, width, finished) {
  if (finished && width < MIN_WIDTH) return { ...current, collapsed: true }
  return { width: clampWidth(width), collapsed: false }
}

/**
 * Picks the route to open after login. It never gives #login.
 * React StrictMode runs the guard twice on the dev server; the second run already sees
 * #login in the address, so it keeps the route saved by the first run (EWS_WEB-51).
 * @returns {string} The current hash, the route saved before, or #dashboard.
 * @throws {DOMException} If sessionStorage is unavailable or access is denied.
 */
function returnRouteToSave() {
  const hash = window.location.hash
  if (hash && hash !== '#login') return hash
  return sessionStorage.getItem('ews-return-route') || '#dashboard'
}

/**
 * Applies the sample-session route guard and marks the decision as ready.
 * Unauthenticated users are sent to Login; authenticated users return to their saved route.
 * @param {{username: string}|null} session - Current sample session.
 * @param {string} page - Resolved page key.
 * @param {(ready: boolean) => void} setGuardReady - Updates the guard rendering state.
 * @returns {void}
 * @throws {DOMException} If sessionStorage is unavailable or access is denied.
 */
export function applyRouteGuard(session, page, setGuardReady) {
  if (!session && page !== 'login') {
    sessionStorage.setItem('ews-return-route', returnRouteToSave())
    window.location.hash = '#login'
    setGuardReady(true)
    return
  }
  if (session && page === 'login') {
    const returnRoute = sessionStorage.getItem('ews-return-route') || '#dashboard'
    sessionStorage.removeItem('ews-return-route')
    window.location.hash = returnRoute
    setGuardReady(true)
    return
  }
  setGuardReady(true)
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
 * Shows the sidebar and the page selected by the browser hash.
 * @returns {JSX.Element} The whole app.
 */
export default function App() {
  const hash = useHash()
  const route = resolveRoute(hash, Object.keys(PAGES))
  const [session, setSession] = useState(getSampleSession)
  const [guardReady, setGuardReady] = useState(false)

  useEffect(() => subscribeToSession(setSession), [])

  useEffect(() => {
    applyRouteGuard(session, route.page, setGuardReady)
  }, [hash, route.page, session])

  const Page = PAGES[route.page]
  const [sidebar, setSidebar] = useState(readSidebar)
  const [resizing, setResizing] = useState(false)
  const phone = useMediaQuery(`(width < ${PHONE_WIDTH}px)`)
  const tablet = useMediaQuery(`(${PHONE_WIDTH}px <= width < ${DESKTOP_WIDTH}px)`)
  const [mobileMenu, { open: openMobileMenu, close: closeMobileMenu }] = useDisclosure(false)
  const collapsed = tablet || sidebar.collapsed
  const sidebarWidth = collapsed ? NARROW_WIDTH : sidebar.width

  // Close the drawer and end any drag when the viewport changes modes.
  useEffect(() => {
    phone || closeMobileMenu()
  }, [phone, closeMobileMenu])

  useEffect(() => {
    (phone || tablet) && setResizing(false)
  }, [phone, tablet])


  useEffect(() => saveSidebar(sidebar, resizing), [sidebar, resizing])

  const showLogin = !guardReady || !session || route.page === 'login'

  /**
   * Switches between the full and the icons-only sidebar.
   * @returns {void}
   */
  function toggleSidebar() {
    setSidebar(toggledSidebar)
  }

  /**
   * Applies a width from the drag handle; a finished drag below MIN_WIDTH collapses the sidebar.
   * @param {number} width - Width from the handle in px.
   * @param {boolean} finished - True on the last step of the drag.
   * @returns {void}
   */
  function resizeSidebar(width, finished) {
    setSidebar((current) => resizedSidebar(current, width, finished))
  }

  if (showLogin) return <LoginPage onLogin={setSession} />

  return (
    <AppShell
      navbar={{ width: sidebarWidth, breakpoint: PHONE_WIDTH, collapsed: { mobile: true } }}
      header={{ height: { base: 56, sm: 0 } }}
      padding={{ base: 'md', sm: 'xl' }}
      transitionDuration={resizing ? 0 : 150}
    >
      <AppShell.Header hiddenFrom="sm" bg="var(--ews-green-900)" withBorder={false}>
        <Group h="100%" px="md">
          <Burger
            color="var(--ews-text-on-dark)"
            aria-label="Open dashboard navigation"
            aria-expanded={mobileMenu}
            onClick={openMobileMenu}
          />
          <Text c="var(--ews-text-on-dark)" fw={700}>EcoWattSense</Text>
        </Group>
      </AppShell.Header>

      <AppShell.Navbar
        visibleFrom="sm"
        className={sidebarClasses.navbar}
        bg="var(--ews-green-900)"
        withBorder={false}
        data-width={sidebarWidth}
      >
        <Sidebar
          items={navigation}
          activeKey={route.menuKey}
          collapsed={collapsed}
          onToggle={tablet ? undefined : toggleSidebar}
          onResize={tablet ? undefined : resizeSidebar}
          onResizingChange={setResizing}
        />
      </AppShell.Navbar>

      <Drawer
        opened={phone && mobileMenu}
        onClose={closeMobileMenu}
        classNames={{
          content: sidebarClasses.drawerContent,
          header: sidebarClasses.drawerHeader,
          close: sidebarClasses.drawerClose,
        }}
        closeButtonProps={{ 'aria-label': 'Close dashboard navigation' }}
        size={300}
        padding={0}
      >
        <Sidebar items={navigation} activeKey={route.menuKey} onNavigate={closeMobileMenu} />
      </Drawer>

      <AppShell.Main className={sidebarClasses.page} bg="var(--ews-page)">
        <Page deviceId={route.deviceId} onLogin={setSession} />
      </AppShell.Main>
    </AppShell>
  )
}
