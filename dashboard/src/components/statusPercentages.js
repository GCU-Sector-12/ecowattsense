// Counts come from summary.counts and sum to total. Round down first, then
// give the remaining percentage points to the largest fractional shares.
// This keeps the displayed whole percentages at 100 rather than 99 or 101.
// Equal remainders keep the legend order; zero-count states stay at zero.
export function statusPercentages(counts, total) {
  if (total === 0) return counts.map(() => 0)

  const shares = counts.map((count) => count / total * 100)
  const percentages = shares.map(Math.floor)
  const remainderOrder = shares
    .map((share, index) => ({ index, remainder: share - percentages[index] }))
    .sort((a, b) => b.remainder - a.remainder || a.index - b.index)
  const remaining = 100 - percentages.reduce((sum, percent) => sum + percent, 0)

  for (const { index } of remainderOrder.slice(0, remaining)) {
    percentages[index] += 1
  }
  return percentages
}
