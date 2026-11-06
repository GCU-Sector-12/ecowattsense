import { describe, expect, it } from 'vitest'
import { statusPercentages } from './statusPercentages.js'

describe('status percentages', () => {
  it.each([
    [[12, 5, 3, 0], [60, 25, 15, 0]],
    [[1, 1, 1, 0], [34, 33, 33, 0]],
    [[1, 1, 1, 3], [17, 17, 16, 50]],
    [[0, 0, 0, 4], [0, 0, 0, 100]],
    [[0, 0, 0, 0], [0, 0, 0, 0]],
  ])('apportions %j to %j', (counts, expected) => {
    expect(statusPercentages(counts, counts.reduce((sum, count) => sum + count, 0)))
      .toEqual(expected)
  })

  it('keeps all small valid distributions at 100%, zero states at 0%, and rounding within 1 point', () => {
    for (let active = 0; active <= 5; active++) {
      for (let idle = 0; idle <= 5; idle++) {
        for (let unused = 0; unused <= 5; unused++) {
          for (let noData = 0; noData <= 5; noData++) {
            const counts = [active, idle, unused, noData]
            const total = counts.reduce((sum, count) => sum + count, 0)
            const percentages = statusPercentages(counts, total)
            expect(percentages.reduce((sum, percent) => sum + percent, 0)).toBe(total ? 100 : 0)
            counts.forEach((count, index) => {
              if (count === 0) expect(percentages[index]).toBe(0)
              else expect(Math.abs(percentages[index] - count / total * 100)).toBeLessThan(1)
            })
          }
        }
      }
    }
  })
})
