import '@testing-library/jest-dom/vitest'
import { afterEach, beforeEach, vi } from 'vitest'

// Mantine needs these browser features, but jsdom does not have them.
window.matchMedia = vi.fn().mockImplementation((query) => ({
  matches: false,
  media: query,
  addEventListener: vi.fn(),
  removeEventListener: vi.fn(),
  addListener: vi.fn(),
  removeListener: vi.fn(),
  dispatchEvent: vi.fn(),
}))

class ResizeObserverStub {
  observe() {}
  unobserve() {}
  disconnect() {}
}
window.ResizeObserver = ResizeObserverStub
window.scrollTo = vi.fn()

// Each test starts clean: no saved theme and no page hash.
afterEach(() => {
  localStorage.clear()
  window.location.hash = ''
})

// No test reaches the real network: the sidebar reads the release manifest, so fetch fails unless a test replaces it.
beforeEach(() => {
  globalThis.fetch = vi.fn(() => Promise.reject(new Error('offline')))
})
