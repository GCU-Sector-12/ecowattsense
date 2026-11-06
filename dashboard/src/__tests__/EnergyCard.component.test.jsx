import { screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import EnergyCard from '../components/EnergyCard.jsx'
import { renderWithMantine } from '../test/render.jsx'

const summary = {
  energy_uj: 3_600_000_000_000, cost_gbp: 1.2, co2_kg: 0.207,
  price_gbp_per_kwh: 0.25, co2_kg_per_kwh: 0.207,
}

it('converts microjoules to kWh and shows API cost and CO2', () => {
  renderWithMantine(<EnergyCard {...summary} />)
  expect(screen.getByText('1.0 kWh')).toBeInTheDocument()
  // Cost is supplied by the API, not recalculated using the current tariff.
  expect(screen.getByText('£1.20')).toBeInTheDocument()
  expect(screen.getByText('0.21 kg')).toBeInTheDocument()
})

it('shows valid zero energy, cost and CO2', () => {
  renderWithMantine(<EnergyCard {...summary} energy_uj={0} cost_gbp={0} co2_kg={0} />)
  expect(screen.getByText('0.0 kWh')).toBeInTheDocument()
  expect(screen.getByText('£0.00')).toBeInTheDocument()
  expect(screen.getByText('0.00 kg')).toBeInTheDocument()
})

it('explains the supplied tariff on hover, including changed rates', async () => {
  renderWithMantine(<EnergyCard {...summary} price_gbp_per_kwh={0.31} co2_kg_per_kwh={0.18} />)
  await userEvent.hover(screen.getByRole('button', { name: 'How the estimate is made' }))
  expect(await screen.findByRole('tooltip'))
    .toHaveTextContent('Estimate. Price 0.31 GBP per kWh, 0.18 kg CO2 per kWh')
})

it('makes the tariff explanation available from the keyboard', async () => {
  renderWithMantine(<EnergyCard {...summary} />)
  await userEvent.tab()
  expect(screen.getByRole('button', { name: 'How the estimate is made' })).toHaveFocus()
  expect(await screen.findByRole('tooltip'))
    .toHaveTextContent('Estimate. Price 0.25 GBP per kWh, 0.207 kg CO2 per kWh')
})
