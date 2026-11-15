// Timeline and report table for HistoryPage, using the history API report fields.
import { Box, Card, Group, Stack, Table, Text, Tooltip } from '@mantine/core'
import StateBadge from '../StateBadge.jsx'
import { stateColors, stateLabels } from '../data/ui.js'
import { formatDuration, historyState, reportTime } from '../data/historyView.js'
import { ujToKwh } from '../data/units.js'
import classes from './HistoryReports.module.css'

/**
 * Shows one report as a focusable timeline block with a state word in its tooltip.
 * @param {object} props - One history report.
 * @returns {JSX.Element} Timeline item; time and state remain accessible without colour.
 */
function TimelineBlock({ report }) {
  const state = historyState(report.state)
  const label = `${reportTime(report.timestamp)} UTC — ${stateLabels[state]}`
  return (
    <Tooltip label={label} events={{ hover: true, focus: true, touch: true }}>
      <Box component="li" tabIndex={0} aria-label={label} className={classes.block}
        style={{ backgroundColor: stateColors[state] }} />
    </Tooltip>
  )
}

/**
 * Shows one report in the table, converting numeric states and API energy units.
 * @param {object} props - One history report.
 * @returns {JSX.Element} Report row with textual state and battery status.
 */
function ReportRow({ report }) {
  return (
    <Table.Tr>
      <Table.Td><time dateTime={report.timestamp}>{reportTime(report.timestamp)}</time></Table.Td>
      <Table.Td><StateBadge state={historyState(report.state)} /></Table.Td>
      <Table.Td>{report.cpu_percent.toFixed(1)}%</Table.Td>
      <Table.Td>{formatDuration(report.idle_seconds)}</Table.Td>
      <Table.Td>{report.on_battery ? 'Yes' : 'No'}</Table.Td>
      <Table.Td>{ujToKwh(report.energy_uj).toFixed(3)} kWh</Table.Td>
    </Table.Tr>
  )
}

/**
 * Renders the daily timeline, a state legend and the reports beneath it.
 * @param {object} props - Ordered history reports; one block is drawn for each report.
 * @returns {JSX.Element} Scrollable, labelled timeline and table.
 */
export default function HistoryReports({ reports }) {
  return (
    <Stack gap="md">
      <Card radius="lg" padding="lg" bg="var(--ews-card)">
        <Text component="h2" fz="lg" fw={700} m={0}>Daily timeline</Text>
        <Text c="var(--ews-text-muted)" size="sm" mb="md">{reports.length} reports · Times shown in UTC</Text>
        <Box className={classes.timelineScroll} role="region" aria-label="Scrollable daily timeline" tabIndex={0}>
          <Box component="ul" aria-label="Daily activity reports" className={classes.timeline}>
            {reports.map(report => <TimelineBlock key={report.timestamp} report={report} />)}
          </Box>
        </Box>
        <Group justify="space-between" mt="xs" aria-hidden="true">
          <Text size="xs" c="var(--ews-text-muted)">00:00 UTC</Text>
          <Text size="xs" c="var(--ews-text-muted)">24:00 UTC</Text>
        </Group>
        <Group gap="sm" mt="md" aria-label="Timeline state legend">
          <StateBadge state="active" /><StateBadge state="idle" /><StateBadge state="unused" />
        </Group>
      </Card>
      <Card radius="lg" padding="lg" bg="var(--ews-card)">
        <Text component="h2" fz="lg" fw={700} m={0} mb="md">Reports</Text>
        <Table.ScrollContainer minWidth={650} type="native" role="region" aria-label="Scrollable history reports" tabIndex={0}>
          <Table aria-label="History reports" verticalSpacing="sm" horizontalSpacing="sm"
            styles={{ th: { color: 'var(--ews-text-muted)' }, tr: { borderColor: 'var(--ews-border)' } }}>
            <Table.Thead><Table.Tr>
              <Table.Th>Time (UTC)</Table.Th><Table.Th>State</Table.Th><Table.Th>CPU</Table.Th>
              <Table.Th>Idle time</Table.Th><Table.Th>On battery</Table.Th><Table.Th>Energy</Table.Th>
            </Table.Tr></Table.Thead>
            <Table.Tbody>{reports.map(report => <ReportRow key={report.timestamp} report={report} />)}</Table.Tbody>
          </Table>
        </Table.ScrollContainer>
      </Card>
    </Stack>
  )
}
