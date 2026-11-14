// Dashboard sidebar: logo, menu links, the collapse button and the resize handle. App.jsx renders it
// in the AppShell navbar and owns the width and the collapsed mode; this component only reports events.
import { ActionIcon, Group, NavLink, Stack, Text, Tooltip } from '@mantine/core'
import { useRef } from 'react'
import classes from './Sidebar.module.css'
import { Home, Monitor, BarChart3, FileText, Settings, Leaf, ChevronLeft, ChevronRight } from 'lucide-react'

// Icon names from the navigation list in data/ui.js mapped to lucide icons.
const ICONS = {
  home: Home,
  monitor: Monitor,
  'bar-chart': BarChart3,
  'file-text': FileText,
  settings: Settings,
}

/**
 * One menu entry: icon and label, or icon only with the label in a tooltip when the sidebar is collapsed.
 * @param {object} props - item (key, label, icon name), active flag and collapsed flag.
 * @returns {JSX.Element} Rendered content.
 */
function SidebarLink({ item, active, collapsed, onNavigate }) {
  const Icon = ICONS[item.icon]
  return (
    <Tooltip label={item.label} position="right" disabled={!collapsed} events={{ hover: true, focus: true, touch: false }}>
      <NavLink
        href={`#${item.key}`}
        onClick={onNavigate}
        aria-label={collapsed ? item.label : undefined}
        label={collapsed ? undefined : item.label}
        leftSection={<Icon size={22} aria-hidden="true" />}
        active={active}
        variant="filled"
        color="ews.6"
        data-collapsed={collapsed || undefined}
        classNames={{ root: classes.link, label: classes.label, section: classes.section, body: classes.linkBody }}
      />
    </Tooltip>
  )
}

/**
 * Renders the dashboard menu, the collapse button and the resize handle.
 * The width and the collapsed mode live in App.jsx; this component only reports events.
 * @param {object} props - items (navigation list), activeKey, collapsed, onToggle(),
 *   onResize(width, finished), onResizingChange(resizing), and onNavigate() for the mobile drawer.
 * @returns {JSX.Element} Rendered content.
 */
export default function Sidebar({ items, activeKey, collapsed = false, onToggle, onResize, onResizingChange, onNavigate }) {
  const drag = useRef(null)

  /**
   * Starts a drag on the handle: remembers the pointer and the current navbar width.
   * Only the main button starts a drag, and only one drag at a time.
   * @param {PointerEvent} event - pointerdown on the handle.
   * @returns {void}
   */
  function startResize(event) {
    if (event.button !== 0 || drag.current) return
    const navbar = event.currentTarget.parentElement.getBoundingClientRect()
    drag.current = { pointerId: event.pointerId, startX: event.clientX, startWidth: navbar.width }
    event.currentTarget.setPointerCapture(event.pointerId)
    onResizingChange(true)
  }

  /**
   * Reports the new width while the pointer moves; on the last step it also ends the drag.
   * @param {PointerEvent} event - pointermove or pointerup on the handle.
   * @param {boolean} finished - True for pointerup.
   * @returns {void}
   */
  function resize(event, finished = false) {
    if (drag.current?.pointerId !== event.pointerId) return
    onResize(drag.current.startWidth + event.clientX - drag.current.startX, finished)
    if (finished) stopResize(event)
  }

  /**
   * Ends the drag and tells the app that resizing has stopped. Ignores pointers of another drag.
   * @param {PointerEvent} event - pointerup, pointercancel or lostpointercapture.
   * @returns {void}
   */
  function stopResize(event) {
    if (drag.current?.pointerId !== event.pointerId) return
    drag.current = null
    onResizingChange(false)
  }

  const ToggleIcon = collapsed ? ChevronRight : ChevronLeft
  return (
    <Stack className={classes.sidebar} h="100%" p={collapsed ? 'xs' : 'md'} justify="space-between">
      <Stack gap="xl">
        <Group
          gap="sm"
          wrap="nowrap"
          pt="xs"
          ps={collapsed ? 0 : 'xs'}
          pe={onNavigate ? 32 : collapsed ? 0 : 'xs'}
          justify={collapsed ? 'center' : undefined}
        >
          <Leaf size={40} color="var(--ews-leaf)" />
          {!collapsed && (
            <div>
              <Text c="white" fz="lg" fw={700} lh={1.2}>
                EcoWattSense
              </Text>
              <Text c="white" fz="sm" lh={1.2}>
                Computer Energy Monitor
              </Text>
            </div>
          )}
        </Group>

        <Stack gap="xs">
          {items.map((item) => (
            <SidebarLink key={item.key} item={item} active={item.key === activeKey} collapsed={collapsed} onNavigate={onNavigate} />
          ))}
        </Stack>
      </Stack>

      <Stack gap="sm">
        {!collapsed && (
          <Group gap="sm" wrap="nowrap" px="xs" pb="xs">
            <Leaf size={40} color="var(--ews-leaf)" />
            <Text c="white" fz="sm" fw={500} lh={1.3}>
              Save Energy,
              <br />A Greener Tomorrow
            </Text>
          </Group>
        )}
        {onToggle && (
          <ActionIcon
            className={classes.toggle}
            variant="subtle"
            aria-label={collapsed ? 'Expand sidebar' : 'Collapse sidebar'}
            aria-expanded={!collapsed}
            onClick={onToggle}
          >
            <ToggleIcon size={22} aria-hidden="true" />
          </ActionIcon>
        )}
      </Stack>

      {!collapsed && onResize && (
        <div
          className={classes.resizeHandle}
          aria-hidden="true"
          onPointerDown={startResize}
          onPointerMove={(event) => resize(event)}
          onPointerUp={(event) => resize(event, true)}
          onPointerCancel={stopResize}
          onLostPointerCapture={stopResize}
        />
      )}
    </Stack>
  )
}
