// Component tests for the sidebar installer block: manifest states, local client states, menu, collapsed mode.
import { expect, it, vi } from 'vitest'
import { fireEvent, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import InstallerBlock from '../components/InstallerBlock.jsx'
import { compareVersions } from '../data/useLatestRelease.js'
import { INSTALLER_DOWNLOAD_URL, LOCAL_CLIENT_VERSION_URL } from '../data/ui.js'
import { renderWithMantine } from '../test/render.jsx'

const TAG_URL = 'https://github.com/GCU-Sector-12/ecowattsense/releases/download/v0.1.3-energy-suite/'
const GOOD_MANIFEST = { version: '0.1.3', zip_url: `${TAG_URL}EcoWattSense-Web-0.1.3.zip` }

/** Answers the manifest request with `manifest` and the local client request with `local` (null: no answer). */
function mockFetch(manifest, local = null) {
  globalThis.fetch = vi.fn((url) => {
    if (url === LOCAL_CLIENT_VERSION_URL && local === null) return Promise.reject(new Error('refused'))
    const body = url === LOCAL_CLIENT_VERSION_URL ? { version: local } : manifest
    return Promise.resolve({ ok: true, json: () => Promise.resolve(body) })
  })
}

it('shows the version and links to the installer of the same release when no client answers', async () => {
  mockFetch(GOOD_MANIFEST)
  renderWithMantine(<InstallerBlock collapsed={false} />)
  expect(await screen.findByText('Version 0.1.3')).toBeInTheDocument()
  expect(screen.getByText('EcoWattSense for Windows')).toBeInTheDocument()
  expect(screen.getByText('Windows 11, 64-bit.')).toBeInTheDocument()
  const link = await screen.findByRole('link', { name: 'Install or update' })
  expect(link).toHaveAttribute('href', `${TAG_URL}EcoWattSense-Web-Setup.exe`)
  const localCall = vi.mocked(globalThis.fetch).mock.calls.find(([url]) => url === LOCAL_CLIENT_VERSION_URL)
  expect(localCall[1].credentials).toBe('omit')
})

it('offers "Update to <latest>" when the local client is older', async () => {
  mockFetch(GOOD_MANIFEST, '0.1.2')
  renderWithMantine(<InstallerBlock collapsed={false} />)
  const link = await screen.findByRole('link', { name: 'Update to 0.1.3' })
  expect(link).toHaveAttribute('href', `${TAG_URL}EcoWattSense-Web-Setup.exe`)
})

it('shows "Up to date" without a download button when the local client is equal or higher', async () => {
  mockFetch(GOOD_MANIFEST, '0.1.3')
  renderWithMantine(<InstallerBlock collapsed={false} />)
  expect(await screen.findByText('Up to date')).toBeInTheDocument()
  expect(screen.queryByRole('link', { name: /install|update/i })).not.toBeInTheDocument()
})

it('compares versions part by part as numbers', () => {
  expect(compareVersions('0.1.10', '0.1.9')).toBeGreaterThan(0)
  expect(compareVersions('0.1', '0.1.0')).toBe(0)
  expect(compareVersions('0.2.0', '0.10.0')).toBeLessThan(0)
})

it('falls back to the constant link and hides the version when the manifest is invalid', async () => {
  mockFetch({ version: '0.1.3; rm', zip_url: 'https://evil.example/a.zip' }, '0.1.3')
  renderWithMantine(<InstallerBlock collapsed={false} />)
  await vi.waitFor(() => expect(globalThis.fetch).toHaveBeenCalled())
  expect(screen.queryByText(/Version/)).not.toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Install or update' })).toHaveAttribute('href', INSTALLER_DOWNLOAD_URL)
})

it('falls back to the constant link when the network fails', async () => {
  renderWithMantine(<InstallerBlock collapsed={false} />)
  await vi.waitFor(() => expect(globalThis.fetch).toHaveBeenCalled())
  expect(screen.queryByText(/Version/)).not.toBeInTheDocument()
  expect(screen.getByRole('link', { name: 'Install or update' })).toHaveAttribute('href', INSTALLER_DOWNLOAD_URL)
})

it('opens the menu with a right click and offers Reinstall and Download installer even when up to date', async () => {
  mockFetch(GOOD_MANIFEST, '0.1.3')
  renderWithMantine(<InstallerBlock collapsed={false} />)
  await screen.findByText('Up to date')
  fireEvent.contextMenu(screen.getByText('EcoWattSense for Windows'))
  const items = await screen.findAllByRole('menuitem')
  expect(items.map((item) => item.textContent)).toEqual(['Reinstall', 'Download installer'])
  expect(items[0]).toHaveAttribute('href', `${TAG_URL}EcoWattSense-Web-Setup.exe`)
  expect(items[1]).toHaveAttribute('href', `${TAG_URL}EcoWattSense-Web-Setup.exe`)
})

it('opens the same menu with the More button', async () => {
  mockFetch(GOOD_MANIFEST)
  const user = userEvent.setup()
  renderWithMantine(<InstallerBlock collapsed={false} />)
  await screen.findByText('Version 0.1.3')
  await user.click(screen.getByRole('button', { name: 'More' }))
  expect(await screen.findByRole('menuitem', { name: 'Reinstall' })).toBeInTheDocument()
})

it('shows only an icon link when the sidebar is collapsed, and its menu opens with a right click', async () => {
  mockFetch(GOOD_MANIFEST)
  renderWithMantine(<InstallerBlock collapsed />)
  const link = screen.getByRole('link', { name: 'Install or update EcoWattSense for Windows' })
  expect(screen.queryByText('EcoWattSense for Windows')).not.toBeInTheDocument()
  await vi.waitFor(() => expect(link).toHaveAttribute('href', `${TAG_URL}EcoWattSense-Web-Setup.exe`))
  fireEvent.contextMenu(link)
  expect(await screen.findByRole('menuitem', { name: 'Reinstall' })).toBeInTheDocument()
})
