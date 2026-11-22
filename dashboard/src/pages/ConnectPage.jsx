// The /connect page. A signed-in admin opens it from the address Setup shows (/connect?code=ABCD-EFGH)
// and approves the computer that started Setup, so nobody types the server key. App.jsx shows it
// without the sidebar. It always asks the server (connectProvider.js); it has no sample data mode.
import { useEffect, useState } from 'react'
import { Alert, Anchor, Button, Center, Group, Paper, Stack, Text, TextInput, Title } from '@mantine/core'
import { computerLabelParts } from '../data/computerLabel.js'
import { approveConnectRequest, loadConnectRequest } from '../data/connectProvider.js'
import { followLink, navigate } from '../usePath.js'

const LOADING = { status: 'loading', request: null }
const INVALID = { status: 'invalid', request: null }
const NO_ANSWER = { status: 'no-answer', request: null }
const APPROVED = { status: 'approved', request: null }

/**
 * Reads the code from the address, trimmed.
 * @returns {string} The value of "code", or an empty string when it is missing or only spaces.
 */
function readAddressCode() {
  return (new URLSearchParams(window.location.search).get('code') ?? '').trim()
}

/**
 * Gives the page state for a failed call. Only a 404 means "this code is not valid".
 * @param {{status?: number}} error - Error from the provider.
 * @returns {{status: string, request: null}} The invalid state for 404, the no-answer state otherwise.
 */
function failureState(error) {
  if (error?.status === 404) return INVALID
  return NO_ANSWER
}

/**
 * Gives the page state for a request the server has read.
 * @param {{approved: boolean}} request - Answer of C2.
 * @returns {{status: string, request: object|null}} Connected for an approved request, else ready to approve.
 */
function readState(request) {
  if (request.approved) return APPROVED
  return { status: 'ready', request }
}

/**
 * Reads the request once and gives the state to the page, unless the page has moved on.
 * @param {string} code - The code from the address.
 * @param {{stale: boolean}} guard - Set to stale when the page no longer waits for this answer.
 * @param {(state: object) => void} setState - Page state setter.
 * @returns {Promise<void>} Resolves after the state is set. Never rejects.
 */
async function runLookup(code, guard, setState) {
  let next
  try {
    next = readState(await loadConnectRequest(code))
  } catch (error) {
    next = failureState(error)
  }
  if (!guard.stale) setState(next)
}

/**
 * Approves the request and moves the page to Connected, or to the matching error line.
 * @param {string} code - The code from the address.
 * @param {object} request - The request being approved, kept on screen while the call runs.
 * @param {(state: object) => void} setState - Page state setter.
 * @returns {Promise<void>} Resolves after the state is set. Never rejects.
 */
async function runApprove(code, request, setState) {
  setState({ status: 'sending', request })
  try {
    await approveConnectRequest(code)
    setState(APPROVED)
  } catch (error) {
    setState(failureState(error))
  }
}

/**
 * Centred card with the page title, like the Login page.
 * @param {{children: import('react').ReactNode}} props - Content of the card.
 * @returns {JSX.Element} The card.
 */
function ConnectFrame({ children }) {
  return (
    <Center mih="100vh" px="md" py="xl" bg="var(--ews-page)">
      <Paper
        w="100%"
        maw={430}
        p={{ base: 'xl', sm: 40 }}
        radius="lg"
        shadow="0 12px 36px rgba(15, 23, 42, 0.08)"
        withBorder
        style={{ borderColor: 'var(--ews-border)' }}
      >
        <Stack gap="lg">
          <Title order={1} ta="center" c="var(--ews-text)">Connect a computer</Title>
          {children}
        </Stack>
      </Paper>
    </Center>
  )
}

/**
 * One labelled value of the request.
 * @param {{label: string, children: import('react').ReactNode}} props - Label and value.
 * @returns {JSX.Element} The label above its value.
 */
function Detail({ label, children }) {
  return (
    <Stack gap={2}>
      <Text size="xs" fw={700} c="var(--ews-text-muted)">{label}</Text>
      <div>{children}</div>
    </Stack>
  )
}

/**
 * The name of the computer: its name, or the existing label for a computer without one.
 * @param {{request: object}} props - The request read with C2.
 * @returns {JSX.Element} The name and, for an unnamed computer, its short id.
 */
function ComputerName({ request }) {
  const { text, shortId } = computerLabelParts({ display_name: request.device_name, device_id: request.device_id })
  return (
    <Text>
      {text}
      {shortId && <Text span c="var(--ews-text-muted)"> {shortId}</Text>}
    </Text>
  )
}

/**
 * The request with its two buttons.
 * @param {{request: object, sending: boolean, onApprove: () => void}} props - The request, whether
 *   the approval is running, and the Approve handler.
 * @returns {JSX.Element} The details, the warning text and the buttons.
 */
function RequestDetails({ request, sending, onApprove }) {
  return (
    <Stack gap="md">
      <Detail label="Computer"><ComputerName request={request} /></Detail>
      <Detail label="Type"><Text>{request.device_type}, {request.os}</Text></Detail>
      <Detail label="Code">
        <Text ff="monospace" fz={32} fw={700} c="var(--ews-text)" style={{ letterSpacing: '0.1em' }}>
          {request.user_code}
        </Text>
      </Detail>
      <Text size="sm" c="var(--ews-text-muted)">
        Approve only if you started Setup on this computer and it shows the same code.
      </Text>
      <Group gap="sm">
        <Button color="ews" onClick={onApprove} loading={sending}>Approve</Button>
        <Button variant="default" onClick={goToComputers}>Cancel</Button>
      </Group>
    </Stack>
  )
}

/**
 * Cancel: opens the Computers page. Nothing is sent to the server.
 * @returns {void}
 */
function goToComputers() {
  navigate('/computers')
}

/**
 * The view after an approval: what to do next and a link to the Computers page.
 * @returns {JSX.Element} The confirmation.
 */
function ConnectedView() {
  return (
    <Stack gap="md">
      <Text>Connected. Go back to Setup.</Text>
      <Anchor href="/computers" onClick={followLink}>Open Computers</Anchor>
    </Stack>
  )
}

/**
 * Chooses what the card shows for the current state.
 * @param {{state: {status: string, request: object|null}, onApprove: () => void}} props - The page
 *   state and the Approve handler.
 * @returns {JSX.Element} Loading line, error line, confirmation or the request.
 */
function ConnectContent({ state, onApprove }) {
  if (state.status === 'loading') return <Text c="var(--ews-text-muted)">Checking the code...</Text>
  if (state.status === 'invalid') {
    return <Alert color="gray" role="alert">This code is not valid or has expired. Click Connect in Setup again.</Alert>
  }
  if (state.status === 'no-answer') return <Alert color="gray" role="alert">The server did not answer. Try again.</Alert>
  if (state.status === 'approved') return <ConnectedView />
  return <RequestDetails request={state.request} sending={state.status === 'sending'} onApprove={onApprove} />
}

/**
 * Starts the lookup when the card appears and stops waiting for it when the card goes away.
 * @param {string} code - The code from the address.
 * @param {(state: object) => void} setState - Page state setter.
 * @returns {void}
 */
function useLookup(code, setState) {
  useEffect(() => {
    const guard = { stale: false }
    runLookup(code, guard, setState)
    return () => { guard.stale = true }
  }, [code, setState])
}

/**
 * The card for a code: reads the request, then shows it for approval.
 * @param {{code: string}} props - The trimmed code from the address.
 * @returns {JSX.Element} The card.
 */
function ApprovalCard({ code }) {
  const [state, setState] = useState(LOADING)
  useLookup(code, setState)

  /** Sends the approval for the request on screen. */
  function approve() {
    runApprove(code, state.request, setState)
  }

  return (
    <ConnectFrame>
      <ConnectContent state={state} onApprove={approve} />
    </ConnectFrame>
  )
}

/**
 * Handles the code form: stops the page reload and, when a code is typed, reports it.
 * @param {import('react').FormEvent<HTMLFormElement>} event - The form submit.
 * @param {string} trimmed - The typed code without leading or trailing spaces.
 * @param {(code: string) => void} onFind - Called with the code when it is not empty.
 * @returns {void}
 */
function submitCode(event, trimmed, onFind) {
  event.preventDefault()
  if (trimmed) onFind(trimmed)
}

/**
 * The card for an address without a code: one field and the Find button.
 * @param {{onFind: (code: string) => void}} props - Called with the trimmed code when Find is used.
 * @returns {JSX.Element} The card.
 */
function CodeCard({ onFind }) {
  const [typed, setTyped] = useState('')
  const trimmed = typed.trim()

  /** Keeps what the admin types. */
  function handleChange(event) {
    setTyped(event.currentTarget.value)
  }

  /** Opens the address of the typed code. A form submit is Find too, so Enter works. */
  function handleSubmit(event) {
    submitCode(event, trimmed, onFind)
  }

  return (
    <ConnectFrame>
      <form onSubmit={handleSubmit}>
        <Stack gap="md">
          <TextInput
            label="Code from Setup"
            name="code"
            autoComplete="off"
            autoCapitalize="characters"
            value={typed}
            onChange={handleChange}
            styles={{ input: { textAlign: 'left' } }}
          />
          <Button type="submit" color="ews" disabled={!trimmed}>Find</Button>
        </Stack>
      </form>
    </ConnectFrame>
  )
}

/**
 * Page for /connect. With a code in the address it shows the request for approval; without one
 * it asks for the code and goes to /connect?code=<code>.
 * @returns {JSX.Element} The page.
 */
export default function ConnectPage() {
  const [code, setCode] = useState(readAddressCode)

  /**
   * Moves the address to the typed code and shows its request. The code is encoded because a
   * person typed it.
   * @param {string} typed - The trimmed code from the field.
   * @returns {void}
   */
  function find(typed) {
    navigate('/connect?code=' + encodeURIComponent(typed))
    setCode(typed)
  }

  if (!code) return <CodeCard onFind={find} />
  return <ApprovalCard key={code} code={code} />
}
