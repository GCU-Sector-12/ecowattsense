// UNIT TEST: checks resolveRoute() and the list of pages with no React rendering.
// Proves: each address gives the right page and menu item, a wrong address (also one that
// starts with "/") is "not-found", and the page names and the page components are one list.
import { PAGES } from '../App.jsx'
import { PAGE_NAMES, resolveRoute } from '../useHash.js'

const NOT_FOUND = { page: 'not-found', menuKey: null }

it('gives the page and the menu item for every address', () => {
  expect(resolveRoute('')).toEqual({ page: 'dashboard', menuKey: 'dashboard' })
  expect(resolveRoute('dashboard')).toEqual({ page: 'dashboard', menuKey: 'dashboard' })
  expect(resolveRoute('computers')).toEqual({ page: 'computers', menuKey: 'computers' })
  expect(resolveRoute('energy')).toEqual({ page: 'energy', menuKey: 'energy' })
  expect(resolveRoute('settings')).toEqual({ page: 'settings', menuKey: 'settings' })
  expect(resolveRoute('login')).toEqual({ page: 'login', menuKey: 'login' })
})

it('opens the history page for #computers/<id> and keeps Computers highlighted', () => {
  expect(resolveRoute('computers/abc')).toEqual({
    page: 'history',
    menuKey: 'computers',
    deviceId: 'abc',
  })
})

it.each(['/', '/computers', '/anything', '//'])(
  'an address that starts with "/" is not found, not the Dashboard: #%s',
  (hash) => {
    expect(resolveRoute(hash)).toEqual(NOT_FOUND)
  },
)

it.each(['nonsense', 'reports', 'energy/extra', 'computers/', 'computers/a/b', 'history', 'not-found'])(
  'any other address is not found: #%s',
  (hash) => {
    expect(resolveRoute(hash)).toEqual(NOT_FOUND)
  },
)

it('has a component for every page name, and no component without a name', () => {
  expect(Object.keys(PAGES).sort()).toEqual([...PAGE_NAMES].sort())
  PAGE_NAMES.forEach((name) => expect(typeof PAGES[name]).toBe('function'))
})
