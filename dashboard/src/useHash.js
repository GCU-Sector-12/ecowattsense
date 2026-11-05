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

// Pages the app can show. Reports is not here: it has no page, so its address is "not found".
const PAGES = ['dashboard', 'computers', 'energy', 'settings', 'login']

/**
 * Turns the hash into the page to show and the menu item to highlight.
 *
 * "" and "#dashboard" open the Dashboard. "#computers/abc" is the history page of computer
 * "abc", and Computers stays highlighted. Any other address is "not-found".
 * We split the address by "/" so a wrong address like "#energy/extra" is not accepted.
 *
 * @param {string} hash - The text after the "#" in the address, for example "computers/abc".
 * @returns {{page: string, menuKey: (string|null), deviceId?: string}} The page name, the menu
 *   item to highlight (null when none), and the computer id for the history page.
 */
export function resolveRoute(hash) {
  const parts = hash.split('/')
  const [first, deviceId] = parts

  if (first === '')
    return { page: 'dashboard', menuKey: 'dashboard' }
  if (parts.length === 2 && first === 'computers' && deviceId) {
    return { page: 'history', menuKey: 'computers', deviceId }
  }
  if (parts.length === 1 && PAGES.includes(first)) return { page: first, menuKey: first }
  return { page: 'not-found', menuKey: null }
}
