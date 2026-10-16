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
