// Check that the dashboard renders responsive grid rules for the cards and charts.
import { screen } from '@testing-library/react'
import DashboardPage from '../pages/DashboardPage.jsx'
import { renderWithMantine } from '../test/render.jsx'

/** Returns the generated responsive rules for the rendered grid element. */
function responsiveStyles(element) {
  const responsiveClass = [...element.classList].find((name) => name.startsWith('__m__'))
  expect(responsiveClass).toBeDefined()
  const styles = [...document.querySelectorAll('style[data-mantine-styles="inline"]')]
  return styles.find((style) => style.textContent.includes(`.${responsiveClass}{`))?.textContent
}

it('lays out one, two and four cards across the dashboard breakpoints', async () => {
  const { container } = renderWithMantine(<DashboardPage />)
  await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })
  const styles = responsiveStyles(container.querySelector('.mantine-SimpleGrid-root'))
  expect(styles).toContain('--sg-cols:1')
  expect(styles).toMatch(/@media[^{}]*48em[^{}]*\{[^{}]*\{--sg-cols:2/)
  expect(styles).toMatch(/@media[^{}]*75em[^{}]*\{[^{}]*\{--sg-cols:4/)
})

it('stacks the charts on small screens and uses an eight to four split on desktop', async () => {
  const { container } = renderWithMantine(<DashboardPage />)
  await screen.findByRole('heading', { name: 'Welcome to EcoWattSense!' })
  const [energy, status] = container.querySelectorAll('.mantine-Grid-col')
  const energyStyles = responsiveStyles(energy)
  const statusStyles = responsiveStyles(status)
  expect(energyStyles).toContain('--col-flex-basis:100%')
  expect(statusStyles).toContain('--col-flex-basis:100%')
  expect(energyStyles).toMatch(/@media[^{}]*64em[^{}]*\{[^{}]*\{[^{}]*--col-flex-basis:calc\(66\.666/)
  expect(statusStyles).toMatch(/@media[^{}]*64em[^{}]*\{[^{}]*\{[^{}]*--col-flex-basis:calc\(33\.333/)
})
