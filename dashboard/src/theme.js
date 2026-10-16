import { createTheme } from '@mantine/core'

// Mantine theme built from the EcoWattSense design tokens
// (static/tokens.css, docs/design/ui-tokens.json). Every Mantine colour needs
// 10 shades (index 0 = lightest, 9 = darkest). Shade 6 is the main shade.

// Brand palette. 4 = leaf #50B54E, 6 = green700 #255E4D, 8 = green900 #133931.
const ews = [
  '#EAF5EC',
  '#D2EBD5',
  '#AEDDB3',
  '#86CD8C',
  '#50B54E',
  '#3E9A4A',
  '#255E4D',
  '#1C4B3E',
  '#133931',
  '#0C2620',
]

// Status colours. Shade 0 = tinted background, shade 6 = main colour.
const active = [
  '#E5F6E7',
  '#CBEDCF',
  '#A6E0AD',
  '#7FD38A',
  '#5AC467',
  '#43B451',
  '#33A73E',
  '#2A8B33',
  '#216E29',
  '#18521F',
]

const idle = [
  '#FDF4DC',
  '#FEEBB5',
  '#FEE08A',
  '#FED55F',
  '#FDCF3A',
  '#FDCB2C',
  '#FDC81B',
  '#D9A80F',
  '#B38A0C',
  '#8C6B09',
]

const unused = [
  '#FDEBEB',
  '#FBD0D0',
  '#F8A9A9',
  '#F58282',
  '#F25B5B',
  '#F03A3C',
  '#EF1C1F',
  '#CC1518',
  '#A61114',
  '#800D10',
]

export const theme = createTheme({
  fontFamily: '"Roboto", "Inter", system-ui, -apple-system, "Segoe UI", sans-serif',
  headings: { fontWeight: '700' },

  // Font sizes from the tokens (xs 13, sm 15, md 17, lg 22, xl 28).
  fontSizes: { xs: '13px', sm: '15px', md: '17px', lg: '22px', xl: '28px' },

  // Spacing in multiples of the 8px base unit.
  spacing: { xs: '8px', sm: '12px', md: '16px', lg: '24px', xl: '32px' },

  // Radius: md = 8px (default), lg = 12px (cards).
  defaultRadius: 'md',
  radius: { xs: '4px', sm: '6px', md: '8px', lg: '12px', xl: '16px' },

  colors: { ews, active, idle, unused },
  primaryColor: 'ews',
  primaryShade: 6,

  // Values that are not Mantine colours but are needed in JS (charts).
  other: {
    chartLine: '#189B2D',
    chartFill: '#ECF9ED',
    chartGrid: '#E6E9ED',
  },
})
