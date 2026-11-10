# EWS_WEB-28: joint work handoff

Status: intentionally partial. Both Zahid Hasan (zhasan301) and Aqdas Abdulquassim (aabdul316) are assigned to the task. Aqdas's work below is still to do.

Branch: `EWS_WEB-28-energy-chart`
Task: https://sector12.youtrack.cloud/issue/EWS_WEB-28

## Implemented in this initial part
- toEnergyChartData() converts 30-minute energy_uj buckets to power_kw and energy_kwh without changing API input.
- formatUkTime() formats Europe/London HH:MM labels, including summer/winter clock changes.
- The chart preserves UTC timestamp keys so repeated autumn clock labels do not collapse distinct buckets.
- An empty series displays No data for this day instead of mounting an empty chart.
- Conversion, UK-time/DST and empty-state tests.

## Aqdas's remaining part
- [ ] Replace the fixed Y axis (0 to 2) with a sensible rounded maximum based on the day's power_kw values; handle all-zero data.
- [ ] Finish four-hour tick placement, checking UK-local clock changes and incomplete days; the current interval of 7 is inherited.
- [ ] Build the detailed tooltip showing bucket time, kW and kWh together. energy_kwh is already available in each transformed point.
- [ ] Add axis/tooltip component tests and check actual hover behaviour in the browser.

## Where to continue
- `dashboard/src/components/EnergyChart.jsx`
- `dashboard/src/components/energyChartData.js`
- `dashboard/src/components/energyChartData.test.js`
- `dashboard/src/__tests__/EnergyChart.component.test.jsx`

Use timestamp as the stable chart key and formatUkTime for display only. toEnergyChartData already supplies power_kw and energy_kwh for the richer tooltip. The current default tooltip only shows its existing power series; the Y axis intentionally still uses the inherited fixed range.

## Working together
Continue on this branch and push additional commits to the same draft PR. No duplicate PR or branch is needed. These three task branches are independent; do not copy changes from another task branch into this one.

This branch includes the tested prerequisites in PR #49 (WEB-25 data module) and PR #50 (WEB-29 donut), starting from commit `8ca3264af5dca526efe3222cc014b470eef34142`. Those prerequisites must land before this PR is merged. Each PR targets `dev`.

From a clean checkout, fetch the branch and switch to it. Run `npm ci`, `npm test` and `npm run build` from `dashboard/` after your changes. Check the latest PR CI result as well as the relevant screens in a real browser.

Keep this PR as a draft until the remaining items are implemented and tested. YouTrack currently prevents In Progress until EWS_WEB-25 is Done, so this task remains Open while that prerequisite awaits review. Once the dependency is Done, move this task to In Progress. When the joint implementation is complete, request the team's review; do not merge without that review.

WEB-26 and WEB-27 both touch DashboardPage and the navigation test file in different places. When integrating them after the prerequisites land, keep both the added EnergyCard props and the ComputerTable preview props/tests if Git asks for conflict resolution.
