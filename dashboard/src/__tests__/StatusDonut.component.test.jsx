import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { screen, waitFor, within } from '@testing-library/react'
import { DonutChart } from '@mantine/charts'
import { MantineProvider } from '@mantine/core'
import { theme } from '../theme.js'
import StatusDonut from '../components/StatusDonut.jsx'
import { renderWithMantine } from '../test/render.jsx'
import { getSummary } from '../data/index.js'

// Observe the chart inputs while still rendering the real Mantine chart.
vi.mock('@mantine/charts', async (importOriginal) => {
  const charts = await importOriginal()
  return { ...charts, DonutChart: vi.fn((props) => <charts.DonutChart {...props} />) }
})

beforeEach(() => {
  vi.clearAllMocks()
  // jsdom has no layout. Give ResponsiveContainer a measured chart area
  // so the real chart renders its SVG rather than stopping at width zero.
  vi.spyOn(Element.prototype, 'getBoundingClientRect')
    .mockImplementation(() => new DOMRect(0, 0, 160, 160))
  vi.stubGlobal('ResizeObserver', class {
    constructor(callback) { this.callback = callback }
    observe(target) {
      this.callback([{ target, contentRect: new DOMRect(0, 0, 160, 160) }])
    }
    unobserve() {}
    disconnect() {}
  })
})

afterEach(() => {
  vi.restoreAllMocks()
  vi.unstubAllGlobals()
})

function legendRows() {
  return within(screen.getByRole('list', { name: 'Computer status breakdown' }))
    .getAllByRole('listitem')
}

function displayedPercentages() {
  return legendRows().map((row) => Number(row.textContent.match(/\((\d+)%\)/)[1]))
}

describe('four-state status donut', () => {
  it('renders the sample counts, four labelled legend rows and the centre total', async () => {
    renderWithMantine(<StatusDonut summary={await getSummary()} />)
    const rows = legendRows()
    expect(rows).toHaveLength(4)
    const expectedRows = ['Active12 (60%)', 'Idle5 (25%)', 'Unused3 (15%)', 'No data0 (0%)']
    expectedRows.forEach((text, index) => expect(rows[index]).toHaveTextContent(text))
    expect(screen.getByRole('img', { name: '20 computers in total' }))
      .toHaveTextContent('20Computers')
    expect(displayedPercentages().reduce((sum, percent) => sum + percent, 0)).toBe(100)
    expect(DonutChart.mock.lastCall[0].data.map(({ value }) => value)).toEqual([12, 5, 3, 0])
  })

  it('uses the required muted token for a non-zero No data segment and legend dot', async () => {
    renderWithMantine(<StatusDonut summary={{
      counts: { active: 12, idle: 5, unused: 2, no_data: 1, total: 20 },
    }} />)
    const row = legendRows()[3]
    expect(row).toHaveTextContent('No data1 (5%)')
    // Inspect the authored token; jsdom does not resolve the app stylesheet's
    // CSS custom properties when toHaveStyle reads computed styles.
    expect(row.querySelector('[aria-hidden="true"]').style.background)
      .toBe('var(--ews-text-muted)')
    await waitFor(() => {
      const chart = screen.getByRole('img', { name: '20 computers in total' })
      expect(chart.querySelector('svg [fill="var(--ews-text-muted)"]')).toBeInTheDocument()
    })
    expect(DonutChart.mock.lastCall[0].data).toEqual([
      { name: 'Active', value: 12, color: 'var(--ews-active)' },
      { name: 'Idle', value: 5, color: 'var(--ews-idle)' },
      { name: 'Unused', value: 2, color: 'var(--ews-unused)' },
      { name: 'No data', value: 1, color: 'var(--ews-text-muted)' },
    ])
  })

  it.each([
    [{ active: 1, idle: 1, unused: 1, no_data: 0, total: 3 }, [34, 33, 33, 0]],
    [{ active: 1, idle: 1, unused: 1, no_data: 3, total: 6 }, [17, 17, 16, 50]],
  ])('keeps the displayed percentages at 100 when rounding is needed', (counts, expected) => {
    renderWithMantine(<StatusDonut summary={{ counts }} />)
    expect(displayedPercentages()).toEqual(expected)
    expect(displayedPercentages().reduce((sum, percent) => sum + percent, 0)).toBe(100)
  })

  it('keeps all four rows at 0 (0%) when there are no computers', () => {
    renderWithMantine(<StatusDonut summary={{
      counts: { active: 0, idle: 0, unused: 0, no_data: 0, total: 0 },
    }} />)
    expect(legendRows()).toHaveLength(4)
    for (const row of legendRows()) expect(row).toHaveTextContent('0 (0%)')
    expect(screen.getByRole('img', { name: '0 computers in total' })).toHaveTextContent('0Computers')
    expect(screen.queryByText(/NaN|Infinity/)).not.toBeInTheDocument()
  })

  it('updates both the chart and legend when summary counts change', () => {
    const { rerender } = renderWithMantine(<StatusDonut summary={{
      counts: { active: 0, idle: 0, unused: 0, no_data: 0, total: 0 },
    }} />)
    const view = <StatusDonut summary={{
      counts: { active: 0, idle: 0, unused: 0, no_data: 4, total: 4 },
    }} />
    // renderWithMantine includes its provider in the tree, so preserve it here.
    rerender(<MantineProvider theme={theme} defaultColorScheme="light">{view}</MantineProvider>)
    expect(displayedPercentages()).toEqual([0, 0, 0, 100])
    expect(legendRows()[3]).toHaveTextContent('No data4 (100%)')
    expect(screen.getByRole('img', { name: '4 computers in total' })).toHaveTextContent('4Computers')
    expect(DonutChart.mock.lastCall[0].data.map(({ value }) => value)).toEqual([0, 0, 0, 4])
  })
})
