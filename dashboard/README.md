# Dashboard

Web page for the person who looks after the computers: every computer, its state now, history and estimated energy.

The dashboard is a React app built with Vite (JavaScript) and the Mantine UI library. The backend stays Python. React was chosen for the dashboard on 7 October 2026. ADR-001 section 8 will record the decision.

## Run it

You need Node.js 20.19 or 22.12 or newer.

```bash
cd dashboard
npm install
npm run dev
```

The page opens at http://localhost:5173. For now it shows sample data from `docs/design/dashboard-sample-data.json`. Requests to `/api` go to the server on http://localhost:8000. Change the port in `vite.config.js` if the server uses another one.

Pages are chosen by the address hash. Use the menu, or open `#computers` or `#settings` directly. The Settings page has the theme switch (Light, Dark, Auto).

## Test it

```bash
cd dashboard
npm test
```

This runs all tests once with Vitest. Use `npm run test:watch` to run them again after every change while you work.

There are three kinds of tests in `src/__tests__/`:

| Kind | File | What it proves |
|---|---|---|
| Unit | `sampleData.unit.test.js` | The sample data adds up: 20 computers, counts per state, shares, 48 chart points. |
| Component | `ComputerTable.component.test.jsx` | The computer table shows every row and shows the state as a word, not only a colour. |
| Integration | `navigation.integration.test.jsx` | The whole app opens Settings from the address and the theme switch changes to dark. |

`src/test/setup.js` holds the browser stubs that Mantine needs in tests. `src/test/render.jsx` renders a component inside the Mantine theme.

To check the look by hand, run `npm run dev`, open the page and try each menu item, the theme switch and the keyboard Tab key.

## Files

- `src/` holds the React code. `pages/` has one file per page, `components/` has the parts.
- `src/theme.js` builds the Mantine theme from the design tokens. `src/dark.css` has the dark values.
- `static/tokens.css` holds the design tokens (colours, sizes, fonts). Use the variables, do not hard-code colours. See `docs/design/ui-style-guide.md`.
