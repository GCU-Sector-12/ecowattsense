// Shared setup for every Vitest file (see vite.config.js): browser stubs and a clean start per test.
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
// Mantine's combobox keyboard navigation scrolls the active option into view.
Element.prototype.scrollIntoView = vi.fn()

// Each test starts clean: no saved theme, and the address is the start address "/".
afterEach(() => {
  localStorage.clear()
  window.history.replaceState(null, '', '/')
})

// No test reaches the real network: the sidebar reads the release manifest, so fetch fails unless a test replaces it.
beforeEach(() => {
  globalThis.fetch = vi.fn(() => Promise.reject(new Error('offline')))
})
