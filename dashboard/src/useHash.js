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

// Pages that are not an address of their own: "history" needs a computer id ("#computers/abc")
// and "not-found" is the answer for a wrong address. So "#history" and "#not-found" are not found.
const NOT_AN_ADDRESS = ['history', 'not-found']

/**
 * Turns the hash into the page to show and the menu item to highlight.
 *
 * "" and "#dashboard" open the Dashboard. "#computers/abc" is the history page of computer
 * "abc", and Computers stays highlighted. Any other address is "not-found".
 * We split the address by "/" so a wrong address like "#energy/extra" is not accepted. An
 * address that starts with "/", like "#/computers", is not found too: only an empty hash is the
 * Dashboard.
 *
 * The names of the pages are written once, in PAGES in App.jsx, and App passes them here.
 *
 * @param {string} hash - The text after the "#" in the address, for example "computers/abc".
 * @param {string[]} pageNames - The names of every page the app has.
 * @returns {{page: string, menuKey: (string|null), deviceId?: string}} The page name, the menu
 *   item to highlight (null when none), and the computer id for the history page.
 */
export function resolveRoute(hash, pageNames) {
  const parts = hash.split('/')
  const [first, deviceId] = parts
  const addressPages = pageNames.filter((name) => !NOT_AN_ADDRESS.includes(name))

  if (hash === '') return { page: 'dashboard', menuKey: 'dashboard' }
  if (parts.length === 2 && first === 'computers' && deviceId) {
    return { page: 'history', menuKey: 'computers', deviceId }
  }
  if (parts.length === 1 && addressPages.includes(first)) return { page: first, menuKey: first }
  return { page: 'not-found', menuKey: null }
}
