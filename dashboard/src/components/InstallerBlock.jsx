// Small block at the bottom of the sidebar: product name, latest version, the install/update action and a menu.
// Used by components/Sidebar.jsx on every page; collapsed mode shows only an icon link with a tooltip.
// The menu opens with a right click, the context-menu key, Shift+F10 (all fire "contextmenu") or the More button.
import { useState } from 'react'
import { ActionIcon, Button, Menu, Stack, Text, Tooltip } from '@mantine/core'
import { Download, MonitorDown, MoreHorizontal, RefreshCw } from 'lucide-react'
import { installState, useLatestRelease } from '../data/useLatestRelease.js'

/**
 * The dropdown with the two always-available downloads.
 * @param {object} props - url (installer link), target (element the menu hangs from), opened, onClose().
 * @returns {JSX.Element} Rendered content.
 */
function InstallerMenu({ url, target, opened, onClose }) {
  /** Mantine reports every open or close; only a close is accepted here, opening is done by openMenu. */
  function handleChange(next) {
    if (!next) onClose()
  }
  return (
    <Menu opened={opened} onChange={handleChange} position="top-start" withinPortal>
      <Menu.Target>{target}</Menu.Target>
      <Menu.Dropdown>
        <Menu.Item component="a" href={url} leftSection={<RefreshCw size={16} aria-hidden="true" />}>
          Reinstall
        </Menu.Item>
        <Menu.Item component="a" href={url} leftSection={<Download size={16} aria-hidden="true" />}>
          Download installer
        </Menu.Item>
      </Menu.Dropdown>
    </Menu>
  )
}

/**
 * The action under the version line: update button, install button, or "Up to date".
 * @param {object} props - state ('install', 'update' or 'current'), url and the latest version.
 * @returns {JSX.Element} Rendered content.
 */
function InstallerAction({ state, url, version }) {
  if (state === 'current') return <Text c="white" fz="sm" fw={500}>Up to date</Text>
  const label = state === 'update' ? `Update to ${version}` : 'Install or update'
  return (
    <Button component="a" href={url} size="compact-sm" variant="white" color="ews.6"
      leftSection={<MonitorDown size={16} aria-hidden="true" />}>
      {label}
    </Button>
  )
}

/**
 * Shows "EcoWattSense for Windows", the version line (when known), the action, the menu and the platform note.
 * @param {object} props - collapsed flag of the sidebar.
 * @returns {JSX.Element} Rendered content.
 */
export default function InstallerBlock({ collapsed }) {
  const release = useLatestRelease()
  const [menuOpened, setMenuOpened] = useState(false)
  const state = installState(release.version, release.localVersion)

  /** Opens the menu from a right click, the context-menu key or Shift+F10 instead of the browser menu. */
  function openMenu(event) {
    event.preventDefault()
    setMenuOpened(true)
  }

  /** Closes the menu. */
  function closeMenu() {
    setMenuOpened(false)
  }

  if (collapsed) {
    const icon = (
      <ActionIcon component="a" href={release.url} variant="subtle" size="lg" c="white"
        aria-label="Install or update EcoWattSense for Windows" style={{ alignSelf: 'center' }}>
        <MonitorDown size={22} aria-hidden="true" />
      </ActionIcon>
    )
    const tipped = (
      <Tooltip label="Install or update EcoWattSense for Windows" position="right" disabled={menuOpened}>
        {icon}
      </Tooltip>
    )
    return (
      <div onContextMenu={openMenu} style={{ alignSelf: 'center' }}>
        <InstallerMenu url={release.url} target={tipped} opened={menuOpened} onClose={closeMenu} />
      </div>
    )
  }

  const more = (
    <ActionIcon variant="subtle" size="sm" c="white" aria-label="More" onClick={openMenu}>
      <MoreHorizontal size={16} aria-hidden="true" />
    </ActionIcon>
  )
  return (
    <Stack gap={4} px="xs" onContextMenu={openMenu}>
      <Stack gap={0} align="flex-start">
        <Text c="white" fz="sm" fw={600} lh={1.2}>EcoWattSense for Windows</Text>
        <InstallerMenu url={release.url} target={more} opened={menuOpened} onClose={closeMenu} />
      </Stack>
      {release.version && <Text c="white" fz="xs" lh={1.2}>Version {release.version}</Text>}
      <InstallerAction state={state} url={release.url} version={release.version} />
      <Text c="white" fz="xs" lh={1.2} opacity={0.7}>Windows 11, 64-bit.</Text>
    </Stack>
  )
}
