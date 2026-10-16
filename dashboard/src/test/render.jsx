import { render } from '@testing-library/react'
import { MantineProvider } from '@mantine/core'
import { theme } from '../theme.js'

// Renders a component inside the same Mantine setup the real app uses.
export function renderWithMantine(ui) {
  return render(
    <MantineProvider theme={theme} defaultColorScheme="light">
      {ui}
    </MantineProvider>,
  )
}
