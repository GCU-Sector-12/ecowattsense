*** Begin Patch
*** Update File: dashboard/src/App.jsx
@@
 function resizedSidebar(current, width, finished) {
   if (finished && width < MIN_WIDTH) return { ...current, collapsed: true }
   return { width: clampWidth(width), collapsed: false }
 }
+
+/**
+ * Saves the current protected route and sends an unauthenticated user to login.
+ * @returns {void}
+ */
+function redirectToLogin() {
+  sessionStorage.setItem('ews-return-route', window.location.hash || '#dashboard')
+  window.location.hash = '#login'
+}
+
+/**
+ * Restores the saved route after a successful sample login, defaulting to Dashboard.
+ * @returns {void}
+ */
+function restoreRequestedRoute() {
+  const returnRoute = sessionStorage.getItem('ews-return-route') || '#dashboard'
+  sessionStorage.removeItem('ews-return-route')
+  window.location.hash = returnRoute
+}
+
+/**
+ * Applies the route guard for the current session and route.
+ * @param {{username: string}|null} session - Current sample session.
+ * @param {string} page - Resolved page key.
+ * @param {() => void} markReady - Callback to allow the resolved page to render.
+ * @returns {void}
+ */
+function applyRouteGuard(session, page, markReady) {
+  if (!session && page !== 'login') {
+    redirectToLogin()
+    markReady()
+    return
+  }
+  if (session && page === 'login') {
+    restoreRequestedRoute()
+    markReady()
+    return
+  }
+  markReady()
+}
@@
   useEffect(() => subscribeToSession(setSession), [])
 
   useEffect(() => {
-    const applyRouteGuard = !session && route.page !== 'login'
-      ? () => {
-          sessionStorage.setItem('ews-return-route', window.location.hash || '#dashboard')
-          window.location.hash = '#login'
-          setGuardReady(true)
-        }
-      : session && route.page === 'login'
-        ? () => {
-            const returnRoute = sessionStorage.getItem('ews-return-route') || '#dashboard'
-            sessionStorage.removeItem('ews-return-route')
-            window.location.hash = returnRoute
-            setGuardReady(true)
-          }
-        : () => setGuardReady(true)
-    applyRouteGuard()
+    applyRouteGuard(session, route.page, () => setGuardReady(true))
   }, [hash, route.page, session])
*** Update File: dashboard/src/data/ui.js
@@
-// TODO EWS_WEB-26: derive Idle/Unused captions from getSettings() values
-// idle_threshold_seconds and unused_threshold_seconds instead of these sample defaults.
 export const stateDisplay = {
*** Update File: dashboard/src/__tests__/sidebar.integration.test.jsx
@@
-function dragSidebar(navbar, start, end) {
+/**
+ * Simulates dragging the sidebar resize handle in the test browser.
+ * @param {HTMLElement} navbar - Sidebar navigation element containing the resize handle.
+ * @param {number} start - Starting horizontal pointer position in px.
+ * @param {number} end - Ending horizontal pointer position in px.
+ * @returns {void}
+ * @throws {TypeError} When the expected resize handle is missing from the sidebar.
+ */
+function dragSidebar(navbar, start, end) {
*** Update File: dashboard/src/__tests__/login.integration.test.jsx
@@
 it('redirects a protected route to login and returns there after a successful login', async () => {
@@
   expect(screen.getByText('admin')).toBeInTheDocument()
 })
+
+it('opens the Dashboard after a successful login when there is no saved return route', async () => {
+  const user = userEvent.setup()
+  window.location.hash = '#login'
+  renderWithMantine(<App />)
+
+  await user.type(await screen.findByRole('textbox', { name: 'Username' }), 'admin')
+  await user.type(screen.getByLabelText('Password'), 'admin12345')
+  await user.click(screen.getByRole('button', { name: 'Log in' }))
+
+  expect(await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })).toBeInTheDocument()
+  expect(window.location.hash).toBe('#dashboard')
+})
*** End Patch