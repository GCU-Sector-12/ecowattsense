# EcoWattSense UI style guide

Date: 1 October 2026. Owner: Dariusz Jaroch. Status: agreed by the team (palette), rest proposed.

The team agreed on the look of the dashboard mockup `dashboard-mockup.png`. This page turns the picture into values that developers can use. Machine-readable copies: `ui-tokens.json` here and `dashboard/static/tokens.css` in the code.

## Palette

Colours were read from the mockup pixels. Use the variable names, not the hex values, in code.

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
| `--ews-button` | `#EAECF0` | Secondary button ("View") |
| `--ews-text` | `#0F172A` | Headings, numbers, body |
| `--ews-text-muted` | `#79818E` | Captions, table header, info icon |
| `--ews-chart-line` / `-fill` | `#189B2D` / `#ECF9ED` | Line chart and area under it |

Rules:

- The three status colours always mean the same thing: green = Active, amber = Idle, red = Unused. Do not use them for anything else (no red "delete" buttons, use a neutral button with a confirmation instead).
- Status is never shown by colour alone. Always add the word (Active, Idle, Unused) or an icon, for colour-blind users.
- Text on the dark green sidebar is white. Text on the pale tints is `--ews-text`.
- Contrast check: amber `#FDC81B` on white is too light for text. Use it for bars, dots and icons only, and write the label in `--ews-text`.

## Typography

The mockup uses a clean sans-serif (Roboto). Proposal: **Roboto** (Google Fonts, free) with Inter and system fonts as fallback. One family, weights 400 and 700.

| Use | Size | Weight |
|---|---|---|
| Page title ("Welcome to EcoWattSense!") | 28px | 700 |
| Card and panel titles | 22px | 700 |
| Big numbers in stat cards | 36px | 700 |
| Body, table cells | 17px | 400 |
| Captions, table header, subtitles | 15px | 400 |
| Small labels | 13px | 400 |

## Layout

- Sidebar 300px, full height, dark green. Logo and product name at the top, "Save Energy, A Greener Tomorrow" at the bottom. Navigation: Dashboard, Computers, Energy Usage, Reports, Settings. Selected item has `--ews-green-700` background and 8px radius.
- Content area on `--ews-page`, 32px padding. Header row: title and subtitle on the left, date and time and user menu on the right.
- Row 1: four stat cards of equal width, 16px gap. Active, Idle, Unused each with icon, big number, label, caption and a progress bar showing the share of all computers. Fourth card: estimated energy today in kWh and estimated cost, with an info icon.
- Row 2: line chart "Energy Usage (Today)" two thirds wide, donut "Computer Status" one third wide with the total in the middle and a legend with counts and percentages.
- Row 3: "Computer List" table: Computer Name, Status (dot plus word), Last Activity, Estimated Power, Actions (View button).
- Cards: white, 12px radius, no border, very light shadow or none. Spacing in multiples of 8px.
- Below 1024px the sidebar collapses to icons; below 768px the stat cards stack two per row, then one.

## Status definitions in the mockup

| State | Caption in mockup | Rule |
|---|---|---|
| Active | Actively being used | input or CPU activity in the last 10 minutes |
| Idle | No activity for 10+ mins | 10 to 60 minutes without activity |
| Unused | No activity for 1+ hour | more than 60 minutes without activity |

These thresholds come from the mockup and still need to be confirmed against the client design (ADR-001, state rules). Settings page should let the admin change them.

## Open points

- Power per computer in the mockup is 90 to 130 W. The Planning Report uses 60 W active, 30 W idle, 25 W unused. One of them has to change. Proposal: keep the mockup as a visual only and take real wattage from the energy model in `server/energy.py`.
- Cost uses an electricity tariff. Make it a setting (default 25p per kWh, UK 2026, see Planning Report source [5]).
- Dark mode: not in the mockup. Not planned for D2.
- Icons: the mockup uses simple line icons. Proposal: Lucide (free, MIT).

## Files for UI developers

- `ui-tokens.json`: all values above as JSON.
- `dashboard-sample-data.json`: the numbers from the mockup (20 computers, 12/5/3, 4.8 kWh, £1.20, 48 chart points, 20 table rows) to build the page before the server exists.
- `dashboard-mockup.png`: the agreed picture.
- `dashboard/static/tokens.css`: the CSS variables, the same values as the JSON.
