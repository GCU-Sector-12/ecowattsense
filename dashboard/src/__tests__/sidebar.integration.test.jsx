// Integration checks for the dashboard sidebar and its saved browser preference.
import { fireEvent, screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import App from '../App.jsx'
import { renderWithMantine } from '../test/render.jsx'
import { createSampleSession, clearSampleSession } from '../auth/sampleSession.js'

/** Creates an authenticated sample session for sidebar tests. */
function setUpSidebarTest() {
  createSampleSession('admin')
}

/** Clears the sample session after sidebar tests. */
function cleanUpSidebarTest() {
  clearSampleSession()
}

beforeEach(setUpSidebarTest)
afterEach(cleanUpSidebarTest)

it('collapses with the keyboard, keeps icon links accessible and restores the mode after remounting', async () => {
  const user = userEvent.setup()
  const view = renderWithMantine(<App />)
  const collapse = screen.getByRole('button', { name: 'Collapse sidebar' })
  const navbar = collapse.closest('nav')
  expect(navbar).toHaveAttribute('data-width', '300')

  collapse.focus()
  await user.keyboard('{Enter}')
  expect(navbar).toHaveAttribute('data-width', '72')
  expect(within(navbar).queryByText('Computers')).not.toBeInTheDocument()
  const computers = within(navbar).getByRole('link', { name: 'Computers' })
  await user.hover(computers)
  expect(await screen.findByRole('tooltip', {}, { timeout: 3000 })).toHaveTextContent('Computers')

  view.unmount()
  renderWithMantine(<App />)
  const expand = screen.getByRole('button', { name: 'Expand sidebar' })
  expect(expand.closest('nav')).toHaveAttribute('data-width', '72')
  expand.focus()
  await user.keyboard(' ')
  expect(expand.closest('nav')).toHaveAttribute('data-width', '300')
  expect(within(expand.closest('nav')).getByText('Computers')).toBeVisible()
})

it('clamps drag resizing, saves the width and snaps closed when dragged below the minimum', async () => {
  const view = renderWithMantine(<App />)
  const navbar = screen.getByRole('button', { name: 'Collapse sidebar' }).closest('nav')
  dragSidebar(navbar, 300, 415)
  expect(navbar).toHaveAttribute('data-width', '360')

  view.unmount()
  renderWithMantine(<App />)
  const restoredNavbar = screen.getByRole('button', { name: 'Collapse sidebar' }).closest('nav')
  expect(restoredNavbar).toHaveAttribute('data-width', '360')

  dragSidebar(restoredNavbar, 360, 200)
  expect(restoredNavbar).toHaveAttribute('data-width', '72')
  expect(screen.getByRole('button', { name: 'Expand sidebar' })).toBeVisible()
})

/**
 * Simulates dragging the sidebar resize handle in the test browser.
 * @param {HTMLElement} navbar - Sidebar navigation element containing the resize handle.
 * @param {number} start - Starting horizontal pointer position in px.
 * @param {number} end - Ending horizontal pointer position in px.
 * @returns {void}
 * @throws {TypeError} If the sidebar does not contain the expected resize handle.
 */
function dragSidebar(navbar, start, end) {
  const handle = navbar.querySelector('div[aria-hidden="true"]')
  /** Returns the mocked sidebar width for the resize calculation. */
  function getMockSidebarBounds() {
    return { width: start }
  }

  /** Stubs browser pointer capture, which jsdom does not provide. */
  function capturePointer() {}

  handle.parentElement.getBoundingClientRect = getMockSidebarBounds
  handle.setPointerCapture = capturePointer
  const pointer = { pointerId: 1, button: 0 }
  fireEvent.pointerDown(handle, { ...pointer, clientX: start })
  fireEvent.pointerMove(handle, { ...pointer, clientX: end })
  fireEvent.pointerUp(handle, { ...pointer, clientX: end })
}
