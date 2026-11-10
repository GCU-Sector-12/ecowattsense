// Tests for consistency between the sample endpoint fixtures.
import { getSummary, getDevices, getDeviceHistory, getSettings } from '../data/index.js'

it('sample endpoint fixtures agree on device counts', async () => {
  const summary = await getSummary()
  const { devices } = await getDevices()
  expect(devices).toHaveLength(summary.counts.total)
  for (const state of ['active', 'idle', 'unused', 'no_data']) {
    expect(devices.filter((device) => device.state_name === state))
      .toHaveLength(summary.counts[state])
  }
  expect(summary.series).toHaveLength(48)
})

it('provides 48 sample history reports and settings', async () => {
  const { devices } = await getDevices()
  const history = await getDeviceHistory(devices[0].device_id)
  expect(history.reports).toHaveLength(48)
  expect((await getSettings()).report_interval_seconds).toBe(300)
})
