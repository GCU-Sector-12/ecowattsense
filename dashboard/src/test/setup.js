import '@testing-library/jest-dom/vitest'
import { afterEach, vi } from 'vitest'

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
