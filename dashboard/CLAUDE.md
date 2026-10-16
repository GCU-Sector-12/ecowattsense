# Dashboard: notes for Claude (and for people)

Claude Code reads this file when it works in `dashboard/`. It tells Claude what happened in this folder that is not visible in the code. Read it before you start. Update it before you finish (see "Handover log" at the end).

Anyone can edit this file. Keep it short, true and in English. Do not put secrets, passwords or private links here. The repository is public.

## What this folder is

The web dashboard of EcoWattSense: every computer, its state (Active, Idle, Unused), history and estimated energy and cost. React 19 + Vite. Project context is in the root `README.md`. The design is in `docs/design/` (mockup, `ui-style-guide.md`, `ui-tokens.json`, `dashboard-sample-data.json`).

## Current state (update this section)

- **Data:** the pages show sample data from `docs/design/dashboard-sample-data.json`. No API calls yet. When the server has `GET` endpoints, replace the JSON import in `src/App.jsx` with `fetch('/api/...')`. The Vite proxy already sends `/api` to port 8000.
- **UI library:** Mantine 9 (`@mantine/core`, `@mantine/charts`, `lucide-react` for icons). This is a **proposal shown as a demo**, not a team decision. Do not spread it into other folders. If the team rejects it, the components in `src/components/` are the only place to change.
- **Theme:** `src/theme.js` builds the Mantine theme from the design tokens. Light is the default. `src/dark.css` has the dark values.
- **Pages:** Dashboard, Computers, Settings (Appearance, theme switch). Energy Usage and Reports have a menu item but no page yet, so they show the dashboard.
- **Routing:** the address hash (`#computers`) read by `src/useHash.js`. It is a stop gap. Move to a real router when a page needs a parameter, for example a computer detail page for the "View" button.
- **Tests:** Vitest, 3 kinds in `src/__tests__/` (unit, component, integration). Run `npm test`.
- **Open points:** the "View" button does nothing. Dark mode is not in the agreed mockup and was not planned for D2. Watt values in the sample data (90 to 130 W) differ from the Planning Report (60/30/25 W).

## How to run and test

```bash
cd dashboard
npm install
npm run dev     # http://localhost:5173
npm test        # run all tests once
```

A change is done when `npm test` is green and you looked at the page in `npm run dev`. A test that was never seen failing proves nothing, so for a bug fix write the test first and see it red.

## Rules for this folder

- **Colours and sizes come from tokens.** Use `var(--ews-...)` from `static/tokens.css` or a Mantine theme colour. No hex values in components. The only place for hex is `src/theme.js` and `src/dark.css`.
- **Status colours mean one thing.** Green Active, amber Idle, red Unused. Never use them for anything else. Always show the state as a word as well, never colour alone. Amber is too light for text.
- **Keep code flat.** No nested ternaries, no deep callbacks, no helper components defined inside other components. Small top-level components, one per file in `src/components/` or `src/pages/`.
- **Comments and UI text in English**, short sentences, no emoji.
- **Do not edit the design files** (`docs/design/ui-*`, mockup, sample data) as a side effect of a UI change. Ask first.

## Git and tasks

- Work on a branch `EWS-NN-short-name` from `dev`. Open a pull request to `dev`. One review. Never push to `main` or `dev` directly.
- Tasks are in YouTrack, project EWS. Put the task number in the branch and the commit.
- Commit and PR texts in English. If Claude helped, keep the `Co-Authored-By: Claude ...` line in the commit. It is our open declaration of AI use.
- Never commit secrets, `.env` files, `dist/` or `node_modules/`.

## Handover log

Add one short entry at the end of every session that changes this folder: date, what changed, what is left, anything the next person or Claude must know. Newest at the bottom. Do not rewrite old entries.

- 2026-10-16: Added the Mantine demo (theme from tokens, sidebar, stat cards, chart, donut, table), hash based pages Dashboard, Computers and Settings, dark theme with a switch on the Settings page, and 3 Vitest tests. Not decided by the team yet: Mantine as the UI library and dark mode. Left: "View" button, real API data, pages for Energy Usage and Reports.
