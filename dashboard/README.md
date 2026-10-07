# Dashboard

Web page for the person who looks after the computers: every computer, its state now, history and estimated energy.

The dashboard is a React app built with Vite (JavaScript). The backend stays Python. React was chosen for the dashboard on 7 October 2026. ADR-001 section 8 will record the decision.

## Run it

You need Node.js 20.19 or 22.12 or newer.

```bash
cd dashboard
npm install
npm run dev
```

The page opens at http://localhost:5173. Requests to `/api` go to the server on http://localhost:8000. Change the port in `vite.config.js` if the server uses another one.

## Build it

```bash
npm run build
```

The result is in `dist/` (not in Git). In production the server serves this folder and the API on the same address, so the code always calls `/api/...` with a relative URL.

## Files

- `src/` holds the React code. `App.jsx` is a placeholder until the design is ready (EWS-31).
- `static/tokens.css` holds the design tokens (colours, sizes, fonts). Use the variables, do not hard-code colours. See `docs/design/ui-style-guide.md`.
