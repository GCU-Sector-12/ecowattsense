# EWS_WEB-27: joint work handoff

Status: intentionally partial. Both Zahid Hasan (zhasan301) and Aqdas Abdulquassim (aabdul316) are assigned to the task. Aqdas's work below is still to do.

Branch: `EWS_WEB-27-computers-table`
Task: https://sector12.youtrack.cloud/issue/EWS_WEB-27

## Implemented in this initial part
- Contract table columns: name with first-8-character ID fallback; four-state badge; relative last seen with a full timestamp title; location/building fallback; OS/type; View link.
- Archived devices are hidden by default. The showArchived input is available for the page control.
- The dashboard preview takes the first five non-archived devices and has an All computers link.
- Component tests for columns, fallback names, four states, archived inputs and the five-row limit; integration test for View navigation.

## Aqdas's remaining part
- [ ] Add name/status header sorting.
- [ ] Add state filter (All/Active/Idle/Unused/No data) and name search on the Computers page, using the displayed fallback name as appropriate.
- [ ] Add the Show archived switch and pass its state as ComputerTable's showArchived prop. Update the page count/subtitle to reflect the chosen list.
- [ ] Add interaction tests for sorting, search/filter combinations and toggling archived devices; verify the dashboard stays a short preview.

## Where to continue
- `dashboard/src/pages/ComputersPage.jsx`
- `dashboard/src/components/ComputerTable.jsx`
- `dashboard/src/__tests__/ComputerTable.component.test.jsx`
- `dashboard/src/__tests__/navigation.integration.test.jsx`

ComputerTable accepts computers, title, referenceTime, showArchived (default false), limit and showAllLink. Keep page controls on ComputersPage or explicitly gate any shared controls so the dashboard preview stays compact. Keep the full list until the archived switch can expose archived records.

## Working together
Continue on this branch and push additional commits to the same draft PR. No duplicate PR or branch is needed. These three task branches are independent; do not copy changes from another task branch into this one.

This branch includes the tested prerequisites in PR #49 (WEB-25 data module) and PR #50 (WEB-29 donut), starting from commit `8ca3264af5dca526efe3222cc014b470eef34142`. Those prerequisites must land before this PR is merged. Each PR targets `dev`.

From a clean checkout, fetch the branch and switch to it. Run `npm ci`, `npm test` and `npm run build` from `dashboard/` after your changes. Check the latest PR CI result as well as the relevant screens in a real browser.

Keep this PR as a draft and the task In Progress until the remaining items are implemented and tested. Then request the team's review; do not merge without that review.

WEB-26 and WEB-27 both touch DashboardPage and the navigation test file in different places. When integrating them after the prerequisites land, keep both the added EnergyCard props and the ComputerTable preview props/tests if Git asks for conflict resolution.
