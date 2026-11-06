# EWS_WEB-26: joint work handoff

Status: intentionally partial. Both Zahid Hasan (zhasan301) and Aqdas Abdulquassim (aabdul316) are assigned to the task. Aqdas's work below is still to do.

Branch: `EWS_WEB-26-stat-cards`
Task: https://sector12.youtrack.cloud/issue/EWS_WEB-26

## Implemented in this initial part
- EnergyCard reads energy_uj, cost_gbp, co2_kg and both tariff fields.
- Dashboard passes those contract fields to the card.
- CO2 display plus a hover, keyboard-focus and touch-enabled tariff tooltip.
- Component tests for conversion, zero values, supplied API cost and tooltip access.

## Aqdas's remaining part
- [ ] Add the visible No data count beneath the state cards (or a fourth state card), including zero/plural wording and the grey token.
- [ ] Read thresholds through getSettings() and derive Idle/Unused captions from idle_threshold_seconds and unused_threshold_seconds; remove the fixed captions from this path.
- [ ] Add tests for state counts/share bars, No data and changed thresholds. Keep the energy-card tests green.

## Where to continue
- `dashboard/src/pages/DashboardPage.jsx`
- `dashboard/src/components/StatCard.jsx`
- `dashboard/src/data/ui.js`
- `dashboard/src/__tests__/EnergyCard.component.test.jsx`

The energy card is complete for this split. DashboardPage currently still gets summary/devices only; add getSettings() there for the caption work. Reuse src/data/units.js. Do not recalculate historic API cost using the current tariff.

## Working together
Continue on this branch and push additional commits to the same draft PR. No duplicate PR or branch is needed. These three task branches are independent; do not copy changes from another task branch into this one.

This branch includes the tested prerequisites in PR #49 (WEB-25 data module) and PR #50 (WEB-29 donut), starting from commit `8ca3264af5dca526efe3222cc014b470eef34142`. Those prerequisites must land before this PR is merged. Each PR targets `dev`.

From a clean checkout, fetch the branch and switch to it. Run `npm ci`, `npm test` and `npm run build` from `dashboard/` after your changes. Check the latest PR CI result as well as the relevant screens in a real browser.

Keep this PR as a draft and the task In Progress until the remaining items are implemented and tested. Then request the team's review; do not merge without that review.

WEB-26 and WEB-27 both touch DashboardPage and the navigation test file in different places. When integrating them after the prerequisites land, keep both the added EnergyCard props and the ComputerTable preview props/tests if Git asks for conflict resolution.
