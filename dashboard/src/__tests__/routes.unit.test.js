// UNIT TEST: checks resolveRoute() and the list of pages with no React rendering.
// Proves: each address gives the right page and menu item, a wrong address (also one that
// starts with "/") is "not-found", and every page in PAGES (App.jsx, the one list) can be opened.
import { PAGES } from '../App.jsx'
import { resolveRoute } from '../useHash.js'

const NOT_FOUND = { page: 'not-found', menuKey: null }
const PAGE_NAMES = Object.keys(PAGES)
const route = (hash) => resolveRoute(hash, PAGE_NAMES)

it('gives the page and the menu item for every address', () => {
  expect(route('')).toEqual({ page: 'dashboard', menuKey: 'dashboard' })
  expect(route('dashboard')).toEqual({ page: 'dashboard', menuKey: 'dashboard' })
  expect(route('computers')).toEqual({ page: 'computers', menuKey: 'computers' })
  expect(route('energy')).toEqual({ page: 'energy', menuKey: 'energy' })
  expect(route('settings')).toEqual({ page: 'settings', menuKey: 'settings' })
  expect(route('login')).toEqual({ page: 'login', menuKey: 'login' })
})

it('opens the history page for #computers/<id> and keeps Computers highlighted', () => {
  expect(route('computers/abc')).toEqual({
    page: 'history',
    menuKey: 'computers',
    deviceId: 'abc',
  })
})

it.each(['/', '/computers', '/anything', '//'])(
  'an address that starts with "/" is not found, not the Dashboard: #%s',
  (hash) => {
    expect(route(hash)).toEqual(NOT_FOUND)
  },
)

it.each(['nonsense', 'reports', 'energy/extra', 'computers/', 'computers/a/b', 'history', 'not-found'])(
  'any other address is not found: #%s',
  (hash) => {
    expect(route(hash)).toEqual(NOT_FOUND)
  },
)

it('a page name that App does not list is not found', () => {
  expect(resolveRoute('energy', ['dashboard'])).toEqual(NOT_FOUND)
})

it('every page in PAGES is a component, and each one that has its own address opens from it', () => {
  const ownAddress = PAGE_NAMES.filter((name) => !['history', 'not-found'].includes(name))
  expect(ownAddress.length).toBeGreaterThan(0)
  PAGE_NAMES.forEach((name) => expect(typeof PAGES[name]).toBe('function'))
  ownAddress.forEach((name) => expect(route(name)).toEqual({ page: name, menuKey: name }))
})
