// HistoryReports: timeline blocks are placed by timestamp inside the UTC day, and the table pages.
import { screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { expect, it, vi } from 'vitest'
import HistoryReports from '../components/HistoryReports.jsx'
import { renderWithMantine } from '../test/render.jsx'

/** @param {string} timestamp - ISO time. @returns {object} One 300 s report. */
function report(timestamp) {
  return { timestamp, interval_seconds: 300, state: 2, idle_seconds: 0, cpu_percent: 1,
    screen_locked: false, on_battery: false, energy_uj: 1, energy_source: 'estimated' }
}

it('places a block by its time in the UTC day', () => {
  renderWithMantine(<HistoryReports reports={[report('2026-11-14T12:00:00Z'), report('2026-11-14T00:00:00Z')]} />)
  const blocks = within(screen.getByRole('list', { name: 'Daily activity reports' })).getAllByRole('listitem')
  const noon = blocks.find(block => block.getAttribute('aria-label').startsWith('12:00'))
  expect(noon.style.left).toBe('50%')
  expect(parseFloat(noon.style.width)).toBeCloseTo(300 / 864, 3)
})

/** @returns {object[]} 60 reports, 5 minutes apart from 00:00. */
function sixty() {
  const start = Date.parse('2026-11-14T00:00:00Z')
  return Array.from({ length: 60 }, (_, i) => report(new Date(start + i * 300000).toISOString()))
}

/** @param {object} extra - Paging props. @returns {object} Controlled paging props with spies. */
function paging(extra = {}) {
  return { page: 1, pageSize: 25, onPage: vi.fn(), onPageSize: vi.fn(), ...extra }
}

it('shows 25 rows by default with the range text', () => {
  renderWithMantine(<HistoryReports reports={sixty()} {...paging()} />)
  expect(within(screen.getByRole('table', { name: 'History reports' })).getAllByRole('row')).toHaveLength(26)
  expect(screen.getAllByText('Showing 1 to 25 of 60 reports')).toHaveLength(2)
})

it('shows 50 rows for page size 50 and the second page rows on page 2', () => {
  const { unmount } = renderWithMantine(<HistoryReports reports={sixty()} {...paging({ pageSize: 50 })} />)
  expect(within(screen.getByRole('table', { name: 'History reports' })).getAllByRole('row')).toHaveLength(51)
  unmount()
  renderWithMantine(<HistoryReports reports={sixty()} {...paging({ page: 3 })} />)
  expect(screen.getAllByText('Showing 51 to 60 of 60 reports')).toHaveLength(2)
  expect(within(screen.getByRole('table', { name: 'History reports' })).getAllByRole('row')).toHaveLength(11)
})

it('reports page and size changes', async () => {
  const props = paging()
  renderWithMantine(<HistoryReports reports={sixty()} {...props} />)
  await userEvent.click(screen.getAllByRole('button', { name: /next page/i })[0])
  expect(props.onPage).toHaveBeenCalledWith(2)
  await userEvent.click(screen.getAllByRole('radio', { name: '50' })[0])
  expect(props.onPageSize).toHaveBeenCalledWith(50)
})
