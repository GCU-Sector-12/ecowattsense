import { Group, NavLink, Stack, Text } from '@mantine/core'
import classes from './Sidebar.module.css'
import { Home, Monitor, BarChart3, FileText, Settings, Leaf } from 'lucide-react'

// Icon names from dashboard-sample-data.json mapped to lucide icons.
const ICONS = {
  home: Home,
  monitor: Monitor,
  'bar-chart': BarChart3,
  'file-text': FileText,
  settings: Settings,
}

function SidebarLink({ item, active }) {
  const Icon = ICONS[item.icon]
  return (
    <NavLink
      href={`#${item.key}`}
      label={item.label}
      leftSection={<Icon size={22} />}
      active={active}
      variant="filled"
      color="ews.6"
      classNames={{ root: classes.link, label: classes.label }}
    />
  )
}

export default function Sidebar({ items, activeKey }) {
  return (
    <Stack h="100%" p="md" justify="space-between">
      <Stack gap="xl">
        <Group gap="sm" wrap="nowrap" pt="xs" px="xs">
          <Leaf size={40} color="var(--ews-leaf)" />
          <div>
            <Text c="white" fz="lg" fw={700} lh={1.2}>
              EcoWattSense
            </Text>
            <Text c="white" fz="sm" lh={1.2}>
              Computer Energy Monitor
            </Text>
          </div>
        </Group>

        <Stack gap="xs">
          {items.map((item) => (
            <SidebarLink key={item.key} item={item} active={item.key === activeKey} />
          ))}
        </Stack>
      </Stack>

      <Group gap="sm" wrap="nowrap" px="xs" pb="xs">
        <Leaf size={40} color="var(--ews-leaf)" />
        <Text c="white" fz="sm" fw={500} lh={1.3}>
          Save Energy,
          <br />A Greener Tomorrow
        </Text>
      </Group>
    </Stack>
  )
}
