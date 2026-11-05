import { useSyncExternalStore } from 'react'

function subscribe(callback) {
  window.addEventListener('hashchange', callback)
  return () => window.removeEventListener('hashchange', callback)
}

function getHash() {
  return window.location.hash.replace('#', '')
}

// Current page key from the URL hash, for example "#computers" gives "computers".
export default function useHash() {
  return useSyncExternalStore(subscribe, getHash)
}

// The one list of every page the app has. App.jsx must have a component for each name (a test
// checks it). To add a page: add its name here and its component in App.jsx.
// Reports is not here: it has no page, so its address is "not found".
export const PAGE_NAMES = ['dashboard', 'computers', 'history', 'energy', 'settings', 'login', 'not-found']

// Pages that are not an address of their own: "history" needs a computer id ("#computers/abc")
// and "not-found" is the answer for a wrong address. So "#history" and "#not-found" are not found.
const NOT_AN_ADDRESS = ['history', 'not-found']

// The pages that open from their own address, for example "#energy".
const ADDRESS_PAGES = PAGE_NAMES.filter((name) => !NOT_AN_ADDRESS.includes(name))

/**
 * Turns the hash into the page to show and the menu item to highlight.
 *
 * "" and "#dashboard" open the Dashboard. "#computers/abc" is the history page of computer
 * "abc", and Computers stays highlighted. Any other address is "not-found".
 * We split the address by "/" so a wrong address like "#energy/extra" is not accepted. An
 * address that starts with "/", like "#/computers", is not found too: only an empty hash is the
 * Dashboard.
 *
 * @param {string} hash - The text after the "#" in the address, for example "computers/abc".
 * @returns {{page: string, menuKey: (string|null), deviceId?: string}} The page name, the menu
 *   item to highlight (null when none), and the computer id for the history page.
 */
export function resolveRoute(hash) {
  const parts = hash.split('/')
  const [first, deviceId] = parts

  if (hash === '') return { page: 'dashboard', menuKey: 'dashboard' }
  if (parts.length === 2 && first === 'computers' && deviceId) {
    return { page: 'history', menuKey: 'computers', deviceId }
  }
  if (parts.length === 1 && ADDRESS_PAGES.includes(first)) return { page: first, menuKey: first }
  return { page: 'not-found', menuKey: null }
}
