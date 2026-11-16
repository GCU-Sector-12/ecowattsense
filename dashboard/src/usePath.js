// Page addresses as plain paths (/computers) instead of hashes (#computers).
// App.jsx reads the page from usePath(); the menu and the links use followLink() and navigate();
// main.jsx calls redirectOldHash() once at start.
import { useSyncExternalStore } from 'react'

// navigate() fires this event, because the browser fires popstate only for Back and Forward.
const PATH_CHANGE = 'ews-pathchange'

// Pages that are not an address of their own: "history" needs a computer id ("/computers/abc")
// and "not-found" is the answer for a wrong address. So "/history" and "/not-found" are not found.
const NOT_AN_ADDRESS = ['history', 'not-found']

/**
 * Starts listening for address changes: Back and Forward (popstate) and navigate().
 * @param {() => void} callback - Called by React when the address changes.
 * @returns {() => void} Stops listening.
 */
function subscribe(callback) {
  window.addEventListener('popstate', callback)
  window.addEventListener(PATH_CHANGE, callback)
  return () => {
    window.removeEventListener('popstate', callback)
    window.removeEventListener(PATH_CHANGE, callback)
  }
}

/**
 * Reads the address without its first "/", for example "/computers/abc" gives "computers/abc".
 * @returns {string} The path after the first "/". "" for the start address "/".
 */
function getPath() {
  return window.location.pathname.slice(1)
}

/**
 * The current page address. React draws the app again when it changes.
 * @returns {string} The path without its first "/", for example "computers".
 */
export default function usePath() {
  return useSyncExternalStore(subscribe, getPath)
}

/**
 * Opens an address of the app without a page reload.
 * @param {string} path - The new address, for example "/computers".
 * @param {{replace?: boolean}} [options] - replace: true changes the current entry in the
 *   browser history instead of adding one, so Back does not return to it.
 * @returns {void}
 */
export function navigate(path, { replace = false } = {}) {
  const method = replace ? 'replaceState' : 'pushState'
  window.history[method](null, '', path)
  window.dispatchEvent(new Event(PATH_CHANGE))
}

/**
 * Click handler for a link to a page of the app: opens the page without a reload.
 * A click with Ctrl, Shift, Alt or Cmd, or with another mouse button, is left to the browser,
 * so "open in a new tab" still works.
 * @param {import('react').MouseEvent<HTMLAnchorElement>} event - The click on the link.
 * @returns {void}
 */
export function followLink(event) {
  const newTabClick = event.button !== 0 || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey
  if (newTabClick || event.defaultPrevented) return
  event.preventDefault()
  navigate(event.currentTarget.getAttribute('href'))
}

/**
 * Moves an old hash address to its plain path once, for example "/#computers" to "/computers",
 * so bookmarks and links from before EWS_WEB-57 still work. The old address is replaced in the
 * history, so Back does not return to it. Only the start address "/" is moved, because old
 * addresses always had the hash there; the query string is kept.
 * @returns {void}
 */
export function redirectOldHash() {
  const { pathname, search, hash } = window.location
  if (pathname !== '/' || !hash) return
  // "#/computers" must not become "//computers": the browser reads that as another web site.
  const path = hash.slice(1).replace(/^\/+/, '')
  window.history.replaceState(null, '', `/${path}${search}`)
}

/**
 * Turns the address into the page to show and the menu item to highlight.
 *
 * "" and "dashboard" open the Dashboard. "computers/abc" is the history page of computer
 * "abc", and Computers stays highlighted. Any other address is "not-found".
 * We split the address by "/" so a wrong address like "energy/extra" is not accepted.
 *
 * The names of the pages are written once, in PAGES in App.jsx, and App passes them here.
 *
 * @param {string} path - The address without its first "/", for example "computers/abc".
 * @param {string[]} pageNames - The names of every page the app has.
 * @returns {{page: string, menuKey: (string|null), deviceId?: string}} The page name, the menu
 *   item to highlight (null when none), and the computer id for the history page.
 */
export function resolveRoute(path, pageNames) {
  const parts = path.split('/')
  const [first, deviceId] = parts
  const addressPages = pageNames.filter((name) => !NOT_AN_ADDRESS.includes(name))

  if (path === '') return { page: 'dashboard', menuKey: 'dashboard' }
  if (parts.length === 2 && first === 'computers' && deviceId) {
    return { page: 'history', menuKey: 'computers', deviceId }
  }
  if (parts.length === 1 && addressPages.includes(first)) return { page: first, menuKey: first }
  return { page: 'not-found', menuKey: null }
}