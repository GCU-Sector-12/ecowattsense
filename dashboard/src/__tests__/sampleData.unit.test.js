// UNIT TEST: checks one small piece of logic with no React and no browser.
// Proves: the sample data is consistent, so the dashboard numbers add up.
import data from '../../../docs/design/dashboard-sample-data.json'

function countByStatus(computers, status) {
  return computers.filter((computer) => computer.status === status).length
}

it('sample data adds up', () => {
  const { summary, computers } = data

  expect(computers).toHaveLength(summary.totalComputers)
  expect(summary.active.count + summary.idle.count + summary.unused.count).toBe(summary.totalComputers)

  expect(countByStatus(computers, 'active')).toBe(summary.active.count)
  expect(countByStatus(computers, 'idle')).toBe(summary.idle.count)
  expect(countByStatus(computers, 'unused')).toBe(summary.unused.count)

  const shares = summary.active.share + summary.idle.share + summary.unused.share
  expect(shares).toBeCloseTo(1)

  expect(data.energyToday.points).toHaveLength(48)
})
