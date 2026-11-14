// Component test for the "Add a Windows computer" card.
import { expect, it } from 'vitest'
import { screen } from '@testing-library/react'
import AddComputerCard from '../components/AddComputerCard.jsx'
import { INSTALLER_DOWNLOAD_URL } from '../data/ui.js'
import { renderWithMantine } from '../test/render.jsx'

it('shows the three steps, the server address and the download link', () => {
  renderWithMantine(<AddComputerCard />)
  expect(screen.getByText('Add a Windows computer')).toBeInTheDocument()
  expect(screen.getByText('Download and run the installer.')).toBeInTheDocument()
  expect(screen.getByText(/Enter this server address and the key\./)).toBeInTheDocument()
  expect(screen.getByText('The computer shows here within 5 minutes.')).toBeInTheDocument()
  expect(screen.getByTestId('server-address')).toHaveTextContent(window.location.origin)
  expect(screen.getByText('Windows 11, 64-bit.')).toBeInTheDocument()
})

it('links the button straight to the installer file', () => {
  renderWithMantine(<AddComputerCard />)
  const link = screen.getByRole('link', { name: 'Download installer' })
  expect(link).toHaveAttribute('href', INSTALLER_DOWNLOAD_URL)
  expect(INSTALLER_DOWNLOAD_URL).toMatch(/\/v0\.1\.1-energy-suite\/EcoWattSense-Web-Setup\.exe$/)
})
