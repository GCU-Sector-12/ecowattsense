import StateBadge from './StateBadge.jsx'
import './App.css'

const STATES = ['active', 'idle', 'unused']

// Placeholder page. The real screens follow the approved design (wireframes, EWS-31).
export default function App() {
  return (
    <div className="layout">
      <aside className="sidebar">EcoWattSense</aside>
      <main className="content">
        <h1>Dashboard</h1>
        <p className="muted">The screens will be built from the approved design.</p>
        <div className="states">
          {STATES.map((state) => (
            <StateBadge key={state} state={state} />
          ))}
        </div>
      </main>
    </div>
  )
}
