import { Center } from '@mantine/core'

// Label for one option of the theme switch: an icon and a word, side by side.
export default function SchemeLabel({ icon: Icon, text }) {
  return (
    <Center style={{ gap: 8 }}>
      <Icon size={16} />
      <span>{text}</span>
    </Center>
  )
}
