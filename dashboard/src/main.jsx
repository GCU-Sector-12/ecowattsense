// Start file of the dashboard, loaded by index.html. It draws <App /> into the page.
import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { MantineProvider } from '@mantine/core'
import '@mantine/core/styles.css'
import '@mantine/charts/styles.css'
import '../static/tokens.css'
import './dark.css'
import './index.css'
import { theme } from './theme.js'
import App from './App.jsx'
import { redirectOldHash } from './usePath.js'

// Before the first draw, so App never sees an old "#computers" address.
redirectOldHash()

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <MantineProvider theme={theme} defaultColorScheme="light">
      <App />
    </MantineProvider>
  </StrictMode>,
)
