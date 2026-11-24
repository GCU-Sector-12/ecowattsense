// Timeline and report table for HistoryPage, using the history API report fields.
import { ActionIcon, Box, Card, Group, Loader, Pagination, SegmentedControl, Stack, Table, Text, Tooltip } from '@mantine/core'
import { ChevronLeft, ChevronRight } from 'lucide-react'
import StateBadge from '../StateBadge.jsx'
import { stateColors, stateLabels } from '../data/ui.js'
import { formatDuration, historyState, rangeText, reportTime, shortDay, timelinePlace } from '../data/historyView.js'
import { ujToKwh, chooseDisplayUnit } from '../data/units.js'
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
        style={{ backgroundColor: stateColors[state], ...timelinePlace(report) }} />
    </Tooltip>
  )
}

/**
 * Shows one report in the table, converting numeric states and API energy units.
 * @param {object} props - One history report.
 * @returns {JSX.Element} Report row with textual state and battery status.
 */
function ReportRow({ report }) {
  const energy = ujToKwh(report.energy_uj)
  const display = chooseDisplayUnit(energy)
  return (
    <Table.Tr>
      <Table.Td><time dateTime={report.timestamp}>{reportTime(report.timestamp)}</time></Table.Td>
      <Table.Td><StateBadge state={historyState(report.state)} /></Table.Td>
      <Table.Td>{report.cpu_percent.toFixed(1)}%</Table.Td>
      <Table.Td>{formatDuration(report.idle_seconds)}</Table.Td>
      <Table.Td>{report.on_battery ? 'Yes' : 'No'}</Table.Td>
      <Table.Td>{(energy * display.multiplier).toFixed(3)} {display.unit}</Table.Td>
    </Table.Tr>
  )
}

const PAGE_SIZES = ['25', '50', '75', '100']
const CONTROL_LABELS = { first: 'First page', previous: 'Previous page', next: 'Next page', last: 'Last page' }

/**
 * Names the arrow buttons of the pager for screen readers.
 * @param {string} control - first, previous, next, last or pages.
 * @returns {object} Props for the button; empty for numbered pages.
 */
function controlProps(control) {
  const label = CONTROL_LABELS[control]
  return label ? { 'aria-label': label } : {}
}

/**
 * Page size selector and page buttons under the reports table.
 * @param {object} props - Controlled paging state: page, pageSize, total and the two callbacks.
 * @returns {JSX.Element} Range text, size selector and page controls.
 */
function TablePaging({ page, pageSize, total, onPage, onPageSize }) {
  return (
    <Group justify="space-between" mt="md" gap="md">
      <Text size="sm" c="var(--ews-text-muted)">{rangeText(page, pageSize, total)}</Text>
      <SegmentedControl aria-label="Rows per page" size="xs" data={PAGE_SIZES}
        value={String(pageSize)} onChange={value => onPageSize(Number(value))} />
      <Pagination getControlProps={controlProps} value={page} onChange={onPage} total={Math.max(1, Math.ceil(total / pageSize))} size="sm" />
    </Group>
  )
}

/**
 * Previous day, the selected day and Next day, shown in the timeline card header.
 * @param {object} props - Selected day, the shift callback, whether Next day is allowed and whether a day is loading.
 * @returns {JSX.Element} Day switcher.
 */
function DaySwitcher({ day, onShiftDay, canNext, loading }) {
  return (
    <Group gap="xs" wrap="nowrap">
      <ActionIcon variant="default" aria-label="Previous day" onClick={() => onShiftDay(-1)}>
        <ChevronLeft size={16} aria-hidden="true" />
      </ActionIcon>
      <Text component="time" dateTime={day} size="sm" fw={600}>{shortDay(day)}</Text>
      <ActionIcon variant="default" aria-label="Next day" disabled={!canNext} onClick={() => onShiftDay(1)}>
        <ChevronRight size={16} aria-hidden="true" />
      </ActionIcon>
      {loading ? <Loader size="xs" aria-label="Loading history" /> : null}
    </Group>
  )
}

/**
 * Renders the daily timeline, a state legend and the reports beneath it.
 * The table is paged only when onPage is given; the timeline always shows every report.
 * @param {object} props - Ordered history reports; one block is drawn for each report.
 * @param {number} [props.page] - 1-based table page.
 * @param {number} [props.pageSize] - Rows per page.
 * @param {Function} [props.onPage] - Receives the new page.
 * @param {Function} [props.onPageSize] - Receives the new page size.
 * @param {string} [props.day] - Selected YYYY-MM-DD; with onShiftDay it shows the day switcher.
 * @param {Function} [props.onShiftDay] - Receives -1 or 1.
 * @param {boolean} [props.canNext] - Whether Next day is enabled.
 * @param {boolean} [props.loading] - A new day is loading; shows a small loader by the day switcher.
 * @returns {JSX.Element} Scrollable, labelled timeline and table.
 */
export default function HistoryReports({ reports, page = 1, pageSize = 25, onPage, onPageSize, day, onShiftDay, canNext, loading }) {
  const paged = Boolean(onPage)
  const rows = paged ? reports.slice((page - 1) * pageSize, page * pageSize) : reports
  return (
    <Stack gap="md">
      <Card radius="lg" padding="lg" bg="var(--ews-card)">
        <Group justify="space-between" gap="md">
          <Text component="h2" fz="lg" fw={700} m={0}>Daily timeline</Text>
          {onShiftDay ? <DaySwitcher day={day} onShiftDay={onShiftDay} canNext={canNext} loading={loading} /> : null}
        </Group>
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
        {paged ? <TablePaging page={page} pageSize={pageSize} total={reports.length}
          onPage={onPage} onPageSize={onPageSize} /> : null}
        <Table.ScrollContainer minWidth={650} type="native" role="region" aria-label="Scrollable history reports" tabIndex={0}>
          <Table aria-label="History reports" verticalSpacing="sm" horizontalSpacing="sm"
            styles={{ th: { color: 'var(--ews-text-muted)' }, tr: { borderColor: 'var(--ews-border)' } }}>
            <Table.Thead><Table.Tr>
              <Table.Th>Time (UTC)</Table.Th><Table.Th>State</Table.Th><Table.Th>CPU</Table.Th>
              <Table.Th>Idle time</Table.Th><Table.Th>On battery</Table.Th><Table.Th>Energy</Table.Th>
            </Table.Tr></Table.Thead>
            <Table.Tbody>{rows.map(report => <ReportRow key={report.timestamp} report={report} />)}</Table.Tbody>
          </Table>
        </Table.ScrollContainer>
        {paged ? <TablePaging page={page} pageSize={pageSize} total={reports.length}
          onPage={onPage} onPageSize={onPageSize} /> : null}
      </Card>
    </Stack>
  )
}
