import { ActionIcon, Group, NavLink, Stack, Text, Tooltip } from '@mantine/core'
import { useRef } from 'react'
import classes from './Sidebar.module.css'
import { Home, Monitor, BarChart3, FileText, Settings, Leaf, ChevronLeft, ChevronRight } from 'lucide-react'

// Icon names from dashboard-sample-data.json mapped to lucide icons.
const ICONS = {
  home: Home,
  monitor: Monitor,
  'bar-chart': BarChart3,
  'file-text': FileText,
  settings: Settings,
}

function SidebarLink({ item, active, collapsed }) {
  const Icon = ICONS[item.icon]
  return (
    <Tooltip label={item.label} position="right" disabled={!collapsed} events={{ hover: true, focus: true, touch: false }}>
      <NavLink
        href={`#${item.key}`}
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

/** Renders the dashboard menu and reports pointer movement from its resize handle. */
export default function Sidebar({ items, activeKey, collapsed, onToggle, onResize, onResizingChange }) {
  const drag = useRef(null)

  function startResize(event) {
    if (event.button !== 0 || drag.current) return
    const navbar = event.currentTarget.parentElement.getBoundingClientRect()
    drag.current = { pointerId: event.pointerId, startX: event.clientX, startWidth: navbar.width }
    event.currentTarget.setPointerCapture(event.pointerId)
    onResizingChange(true)
  }

  function resize(event, finished = false) {
    if (drag.current?.pointerId !== event.pointerId) return
    onResize(drag.current.startWidth + event.clientX - drag.current.startX, finished)
    if (finished) stopResize(event)
  }

  function stopResize(event) {
    if (drag.current?.pointerId !== event.pointerId) return
    drag.current = null
    onResizingChange(false)
  }

  const ToggleIcon = collapsed ? ChevronRight : ChevronLeft
  return (
    <Stack className={classes.sidebar} h="100%" p={collapsed ? 'xs' : 'md'} justify="space-between">
      <Stack gap="xl">
        <Group gap="sm" wrap="nowrap" pt="xs" px={collapsed ? 0 : 'xs'} justify={collapsed ? 'center' : undefined}>
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
            <SidebarLink key={item.key} item={item} active={item.key === activeKey} collapsed={collapsed} />
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
        <ActionIcon
          className={classes.toggle}
          variant="subtle"
          aria-label={collapsed ? 'Expand sidebar' : 'Collapse sidebar'}
          aria-expanded={!collapsed}
          onClick={onToggle}
        >
          <ToggleIcon size={22} aria-hidden="true" />
        </ActionIcon>
      </Stack>

      {!collapsed && (
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
