// Check that the dashboard renders responsive grid rules for the cards and charts.
import data from '../mock/dashboard-sample-data.json'
import DashboardPage from '../pages/DashboardPage.jsx'
import { renderWithMantine } from '../test/render.jsx'

function responsiveStyles(element) {
  const responsiveClass = [...element.classList].find((name) => name.startsWith('__m__'))
  expect(responsiveClass).toBeDefined()
  const styles = [...document.querySelectorAll('style[data-mantine-styles="inline"]')]
  return styles.find((style) => style.textContent.includes(`.${responsiveClass}{`))?.textContent
}

it('lays out one, two and four cards across the dashboard breakpoints', () => {
  const { container } = renderWithMantine(<DashboardPage data={data} />)
  const styles = responsiveStyles(container.querySelector('.mantine-SimpleGrid-root'))
  expect(styles).toContain('--sg-cols:1')
  expect(styles).toMatch(/@media[^{}]*48em[^{}]*\{[^{}]*\{--sg-cols:2/)
  expect(styles).toMatch(/@media[^{}]*75em[^{}]*\{[^{}]*\{--sg-cols:4/)
})

it('stacks the charts on small screens and uses an eight to four split on desktop', () => {
  const { container } = renderWithMantine(<DashboardPage data={data} />)
  const [energy, status] = container.querySelectorAll('.mantine-Grid-col')
  const energyStyles = responsiveStyles(energy)
  const statusStyles = responsiveStyles(status)
  expect(energyStyles).toContain('--col-flex-basis:100%')
  expect(statusStyles).toContain('--col-flex-basis:100%')
  expect(energyStyles).toMatch(/@media[^{}]*64em[^{}]*\{[^{}]*\{[^{}]*--col-flex-basis:calc\(66\.666/)
  expect(statusStyles).toMatch(/@media[^{}]*64em[^{}]*\{[^{}]*\{[^{}]*--col-flex-basis:calc\(33\.333/)
})
