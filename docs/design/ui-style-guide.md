# EcoWattSense UI style guide

Date: 15 November 2026. Owner: Dariusz Jaroch. Status: agreed by the team (palette); implementation notes updated after the dashboard page audit.

The team agreed on the look of the dashboard mockup [`dashboard-mockup.png`](dashboard-mockup.png). This page turns the picture into values that developers can use. Machine-readable copies: [`ui-tokens.json`](ui-tokens.json) and [`dashboard/static/tokens.css`](../../dashboard/static/tokens.css).

## Palette

Colours were read from the mockup pixels. Use the variable names, not the hex values, in component code.

| Token | Hex | Where it is used |
|---|---|---|
| `--ews-green-900` | `#133931` | Sidebar background, dark surfaces |
| `--ews-green-700` | `#255E4D` | Selected item in the sidebar |
| `--ews-leaf` | `#50B54E` | Logo leaf, accent on dark background |
| `--ews-active` / `-bg` | `#33A73E` / `#E5F6E7` | Active state: icon, bar, dot, card tint |
| `--ews-idle` / `-bg` | `#FDC81B` / `#FDF4DC` | Idle state |
| `--ews-unused` / `-bg` | `#EF1C1F` / `#FDEBEB` | Unused state |
| `--ews-page` | `#F7F9FB` | Page background |
| `--ews-card` | `#FFFFFF` | Cards, chart panels, table |
| `--ews-card-alt` | `#F0F2F5` | Summary card (energy today) |
| `--ews-track` | `#D8DCDD` | Progress bar track |
| `--ews-border` | `#E6E9ED` | Table row lines, card outline |
| `--ews-button` | `#EAECF0` | Secondary button (View) |
| `--ews-text` | `#0F172A` | Headings, numbers, body |
| `--ews-text-muted` | `#79818E` | Captions, table header, info icon |
| `--ews-chart-line` / `-fill` | `#189B2D` / `#ECF9ED` | Line chart and area under it |

Rules:

- The three status colours always mean the same thing: green = Active, amber = Idle, red = Unused. Do not use them for anything else (no red delete buttons; use a neutral button with confirmation instead).
- Status is never shown by colour alone. Always add the word (Active, Idle, Unused, No data) or an icon, for colour-blind users.
- Text on the dark green sidebar is white. Text on pale tints is `--ews-text`.
- Amber `#FDC81B` on white is too light for text. Use it for bars, dots and icons only, and write the label in `--ews-text`.
- Component colours use CSS variables from `dashboard/static/tokens.css`. The Mantine palette in `dashboard/src/theme.js` is the central theme adapter; do not add colour literals to page or component files.

## Typography

The mockup uses a clean sans-serif (Roboto). Use **Roboto** with Inter and system fonts as fallback. One family, weights 400 and 700.

| Use | Size | Weight |
|---|---|---|
| Page title | 28px | 700 |
| Card and panel titles | 22px | 700 |
| Big numbers in stat cards | 36px | 700 |
| Body, table cells | 17px | 400 |
| Captions, table header, subtitles | 15px | 400 |
| Small labels | 13px | 400 |

## Layout

- Sidebar: 300px full height, dark green. Logo and product name at the top; “Save Energy, A Greener Tomorrow” at the bottom.
- Current navigation: **Dashboard, Computers, Energy Usage, Settings**. Reports was in the original mock-up proposal but has no implemented page and is intentionally removed from the application navigation.
- Selected navigation item uses `--ews-green-700` and an 8px radius.
- Content area uses `--ews-page` and 32px outer padding. Header: page title and subtitle on the left; date/time and user menu on the right.
- Dashboard row 1: four equal stat cards with a 16px gap. Active, Idle and Unused cards show icon, count, label, caption and progress bar. The fourth card shows estimated energy, cost and CO₂.
- Dashboard row 2: energy usage chart and computer status donut. They stack below 1024px.
- Dashboard row 3: Computer List table with computer name, status (dot plus word), last seen, location, OS/type and View action. Location and OS/type columns hide below 768px; the table scrolls horizontally on small screens.
- Cards: white (or dark-scheme surface), 12px radius, no border, very light shadow or none. Use spacing values that are multiples of the 8px base unit. The dashboard grid gap is 16px.
- Below 1024px the sidebar collapses to icons; below 768px it is behind a burger menu and stat cards stack two per row, then one on narrow phones.
- Use the token spacing scale in `dashboard/src/theme.js`: 8, 16, 16, 24 and 32px. Do not introduce off-grid spacing for layout.

## Status definitions in the mockup

| State | Caption in mockup | Rule |
|---|---|---|
| Active | Actively being used | input or CPU activity in the last 10 minutes |
| Idle | No activity for 10+ mins | 10 to 60 minutes without activity |
| Unused | No activity for 1+ hour | more than 60 minutes without activity |

These thresholds come from the mockup and still need to be confirmed against the client design (ADR-001, state rules). Settings page should let the admin change them.

## Implemented dashboard behaviour

- **Dark mode exists.** The Settings page has Light, Dark and Auto appearance choices. Dark mode changes the page, card, text, border, track and chart-fill tokens; brand and status meanings stay consistent.
- **No data state exists.** The shared `DataState` component presents a neutral “No data” message for empty data regions, separate from loading skeletons and error/retry feedback.
- **Pages outside the original dashboard mock-up:** Computers adds search, state filtering, archive visibility and sorting; Energy Usage provides daily energy data; History provides per-computer daily details and reports; Login provides authenticated entry; Settings provides appearance controls and report interval; Not Found handles unknown routes.
- The original mock-up's sample figures are fixtures, not measured energy data. Use the API contract and energy model for real values.

## Open points

- Power per computer in the mockup is 90 to 130 W. The Planning Report uses 60 W active, 30 W idle, 25 W unused. One of them has to change. Proposal: keep the mockup as a visual only and take real wattage from the energy model in `server/energy.py`.
- Cost uses an electricity tariff. Make it a setting (default 25p per kWh, UK 2026, see Planning Report source [5]).
- Icons: the mockup uses simple line icons. The dashboard uses Lucide icons.

## Files for UI developers

- `dashboard-mockup.png`: the agreed picture.
- `dashboard-sample-data.json`: sample figures from the mockup (20 computers, 12/5/3, 4.8 kWh, £1.20, chart points and table rows). These are made-up UI fixtures, not live measurements.
- `ui-tokens.json`: machine-readable design values.
- `dashboard/static/tokens.css`: CSS variables used by dashboard components.
