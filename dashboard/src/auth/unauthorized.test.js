// Unit tests for endSessionOn401.
import { afterEach, expect, it, vi } from 'vitest'
import { endSessionOn401 } from './unauthorized.js'

afterEach(() => {
  vi.restoreAllMocks()
  sessionStorage.clear()
})

it('clears the session on 401', () => {
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'admin' }))
  endSessionOn401({ status: 401 })
  expect(sessionStorage.getItem('ews-sample-session')).toBeNull()
})

it('keeps the session on other statuses', () => {
  sessionStorage.setItem('ews-sample-session', JSON.stringify({ username: 'admin' }))
  endSessionOn401({ status: 500 })
  expect(sessionStorage.getItem('ews-sample-session')).not.toBeNull()
})

it('does not throw when sessionStorage fails', () => {
  vi.spyOn(Storage.prototype, 'removeItem').mockImplementation(() => { throw new Error('denied') })
  expect(() => endSessionOn401({ status: 401 })).not.toThrow()
})
