// Tests for the shared computer label rule.
import { computerLabel, computerLabelParts } from './computerLabel.js'

const id = '12345678-aaaa'

it('uses the trimmed name when there is one', () => {
  expect(computerLabel({ display_name: '  Lab PC ', device_id: id })).toBe('Lab PC')
})

it.each([null, undefined, '', '   '])('labels %j as unnamed with the short id', (name) => {
  expect(computerLabel({ display_name: name, device_id: id })).toBe('Unnamed computer (12345678)')
  expect(computerLabelParts({ display_name: name, device_id: id })).toEqual({ text: 'Unnamed computer', shortId: '12345678' })
})

it('does not throw when device_id is missing', () => {
  expect(computerLabel({ display_name: null })).toBe('Unnamed computer ()')
})
