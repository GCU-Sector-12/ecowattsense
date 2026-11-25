import { render } from '@testing-library/react'
import { MantineProvider } from '@mantine/core'
import { theme } from '../theme.js'

/**
 * Renders a component with the application theme and test-safe Mantine behavior.
 * @param {React.ReactNode} ui - Component tree to render.
 * @returns {object} React Testing Library render utilities.
 */
export function renderWithMantine(ui) {
  return render(
    <MantineProvider theme={theme} defaultColorScheme="light" env="test">
      {ui}
    </MantineProvider>,
  )
}
