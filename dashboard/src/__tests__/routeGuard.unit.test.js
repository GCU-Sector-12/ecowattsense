// Unit test for the route guard when React StrictMode runs its effect twice (EWS_WEB-51).
// The Vite dev server uses StrictMode. The guard then runs twice before the page changes,
// and the second run must not save #login as the return route.
import { vi } from 'vitest'
import { applyRouteGuard } from '../App.jsx'

/** Clears the stored routes and the hash before and after each test. */
function resetRoutes() {
  sessionStorage.clear()
  window.location.hash = ''
}

/**
 * Runs the guard twice for a visitor without a session on the bare address, as StrictMode does.
 * @returns {void} Checks that the saved return route is the dashboard.
 */
function keepsDashboardAsReturnRouteWhenGuardRunsTwice() {
  const setGuardReady = vi.fn()

  applyRouteGuard(null, 'dashboard', setGuardReady)
  applyRouteGuard(null, 'dashboard', setGuardReady)

  expect(window.location.hash).toBe('#login')
  expect(sessionStorage.getItem('ews-return-route')).toBe('#dashboard')
}

/**
 * Runs the guard twice for a protected page, as StrictMode does.
 * @returns {void} Checks that the saved return route is the requested page.
 */
function keepsRequestedPageAsReturnRouteWhenGuardRunsTwice() {
  const setGuardReady = vi.fn()
  window.location.hash = '#computers'

  applyRouteGuard(null, 'computers', setGuardReady)
  applyRouteGuard(null, 'computers', setGuardReady)

  expect(sessionStorage.getItem('ews-return-route')).toBe('#computers')
}

beforeEach(resetRoutes)
afterEach(resetRoutes)

it('keeps #dashboard as the return route when the guard runs twice on the bare address', keepsDashboardAsReturnRouteWhenGuardRunsTwice)
it('keeps the requested page as the return route when the guard runs twice', keepsRequestedPageAsReturnRouteWhenGuardRunsTwice)
