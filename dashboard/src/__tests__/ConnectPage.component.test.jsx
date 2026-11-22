// COMPONENT TEST: the /connect page (ConnectPage.jsx) with the connect provider mocked.
// Proves: the request is shown and can be approved, every failure has its one text, a missing code
// shows the code field, the typed code is trimmed and encoded, and Cancel calls nothing.
import { beforeEach, expect, it, vi } from 'vitest'
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import ConnectPage from '../pages/ConnectPage.jsx'
import { approveConnectRequest, loadConnectRequest } from '../data/connectProvider.js'
import { renderWithMantine } from '../test/render.jsx'

vi.mock('../data/connectProvider.js', () => ({
  loadConnectRequest: vi.fn(),
  approveConnectRequest: vi.fn(),
}))

const request = {
  user_code: 'ABCD-EFGH',
  device_id: '11111111-2222-3333-4444-555555555555',
  device_name: 'PC-01',
  os: 'windows',
  device_type: 'laptop',
  approved: false,
  expires_in_seconds: 412,
}

const NOT_VALID = 'This code is not valid or has expired. Click Connect in Setup again.'
const NO_ANSWER = 'The server did not answer. Try again.'

/**
 * Builds an error like the provider throws for a failed HTTP call.
 * @param {number} status - HTTP status.
 * @returns {Error} Error with the status.
 */
function httpError(status) {
  return Object.assign(new Error('HTTP ' + status), { status })
}

/**
 * Opens the page at an address and renders it.
 * @param {string} search - Query string of the address, for example "?code=ABCD-EFGH".
 * @returns {ReturnType<typeof userEvent.setup>} The user-event session of the test.
 */
function openPage(search) {
  window.history.replaceState(null, '', '/connect' + search)
  renderWithMantine(<ConnectPage />)
  return userEvent.setup()
}

/** Starts every test with a readable request and an approval that works. */
function prepare() {
  vi.clearAllMocks()
  loadConnectRequest.mockResolvedValue(request)
  approveConnectRequest.mockResolvedValue(undefined)
}
beforeEach(prepare)

/** Checks the whole request view. */
async function showsTheRequest() {
  openPage('?code=ABCD-EFGH')
  expect(await screen.findByRole('heading', { name: 'Connect a computer' })).toBeInTheDocument()
  expect(await screen.findByText('PC-01')).toBeInTheDocument()
  expect(screen.getByText('laptop, windows')).toBeInTheDocument()
  expect(screen.getByText('ABCD-EFGH')).toBeInTheDocument()
  expect(screen.getByText('Approve only if you started Setup on this computer and it shows the same code.')).toBeInTheDocument()
  expect(screen.getByRole('button', { name: 'Approve' })).toBeEnabled()
  expect(screen.getByRole('button', { name: 'Cancel' })).toBeInTheDocument()
  expect(loadConnectRequest).toHaveBeenCalledWith('ABCD-EFGH')
}
it('shows the computer, its type and the code when C2 answers 200', showsTheRequest)

/** Checks the text while C2 has not answered yet. */
async function showsLoading() {
  loadConnectRequest.mockReturnValue(new Promise(() => {}))
  openPage('?code=ABCD-EFGH')
  expect(await screen.findByText('Checking the code...')).toBeInTheDocument()
  expect(screen.queryByRole('button', { name: 'Approve' })).not.toBeInTheDocument()
}
it('shows a checking line while the server has not answered', showsLoading)

/** Checks the label of a computer without a name. */
async function showsUnnamedLabel() {
  loadConnectRequest.mockResolvedValue({ ...request, device_name: null })
  openPage('?code=ABCD-EFGH')
  expect(await screen.findByText('Unnamed computer')).toBeInTheDocument()
  expect(screen.getByText('11111111')).toBeInTheDocument()
}
it('uses the existing label for a computer without a name', showsUnnamedLabel)

/** Checks approve: the call, then the connected view with its link. */
async function approvesTheRequest() {
  const user = openPage('?code=ABCD-EFGH')
  await user.click(await screen.findByRole('button', { name: 'Approve' }))
  expect(await screen.findByText('Connected. Go back to Setup.')).toBeInTheDocument()
  expect(approveConnectRequest).toHaveBeenCalledWith('ABCD-EFGH')
  expect(screen.getByRole('link', { name: 'Open Computers' })).toHaveAttribute('href', '/computers')
  expect(screen.queryByRole('button', { name: 'Approve' })).not.toBeInTheDocument()
}
it('approves with C3 and then shows Connected with a link to Computers', approvesTheRequest)

/** Checks a request that is already approved. */
async function showsConnectedWhenApproved() {
  loadConnectRequest.mockResolvedValue({ ...request, approved: true })
  openPage('?code=ABCD-EFGH')
  expect(await screen.findByText('Connected. Go back to Setup.')).toBeInTheDocument()
  expect(screen.queryByRole('button', { name: 'Approve' })).not.toBeInTheDocument()
  expect(approveConnectRequest).not.toHaveBeenCalled()
}
it('shows Connected at once for a request that is already approved', showsConnectedWhenApproved)

/** Checks the 404 text of C2. */
async function showsNotValidOnRead() {
  loadConnectRequest.mockRejectedValue(httpError(404))
  openPage('?code=ABCD-EFGH')
  expect(await screen.findByText(NOT_VALID)).toBeInTheDocument()
  expect(screen.queryByRole('button', { name: 'Approve' })).not.toBeInTheDocument()
}
it('shows the not valid text for a 404 from C2', showsNotValidOnRead)

/** Checks the 404 text of C3. */
async function showsNotValidOnApprove() {
  approveConnectRequest.mockRejectedValue(httpError(404))
  const user = openPage('?code=ABCD-EFGH')
  await user.click(await screen.findByRole('button', { name: 'Approve' }))
  expect(await screen.findByText(NOT_VALID)).toBeInTheDocument()
}
it('shows the not valid text for a 404 from C3', showsNotValidOnApprove)

/**
 * Checks the one error line of C2.
 * @param {Error} error - Failure of the read.
 */
async function showsNoAnswerOnRead(error) {
  loadConnectRequest.mockRejectedValue(error)
  openPage('?code=ABCD-EFGH')
  expect(await screen.findByText(NO_ANSWER)).toBeInTheDocument()
}
it.each([[new TypeError('Failed to fetch')], [httpError(500)]])(
  'shows the server did not answer line when C2 fails: %s', showsNoAnswerOnRead,
)

/**
 * Checks the one error line of C3 and that the page stays usable.
 * @param {Error} error - Failure of the approval.
 */
async function showsNoAnswerOnApprove(error) {
  approveConnectRequest.mockRejectedValue(error)
  const user = openPage('?code=ABCD-EFGH')
  await user.click(await screen.findByRole('button', { name: 'Approve' }))
  expect(await screen.findByText(NO_ANSWER)).toBeInTheDocument()
}
it.each([[new TypeError('Failed to fetch')], [httpError(500)]])(
  'shows the server did not answer line when C3 fails: %s', showsNoAnswerOnApprove,
)

/** Checks that a code with spaces and small letters is sent trimmed and as typed. */
async function sendsTheCodeTrimmed() {
  openPage('?code=%20abcd-efgh%20')
  await screen.findByText('PC-01')
  expect(loadConnectRequest).toHaveBeenCalledWith('abcd-efgh')
}
it('sends a code with spaces and small letters trimmed', sendsTheCodeTrimmed)

/** Checks Cancel: it goes to Computers and calls nothing more. */
async function cancelCallsNothing() {
  const user = openPage('?code=ABCD-EFGH')
  await user.click(await screen.findByRole('button', { name: 'Cancel' }))
  expect(window.location.pathname).toBe('/computers')
  expect(approveConnectRequest).not.toHaveBeenCalled()
  expect(loadConnectRequest).toHaveBeenCalledTimes(1)
}
it('goes to the Computers page on Cancel and calls nothing', cancelCallsNothing)

/** Checks the page without a code: the field, its label and no server call. */
async function showsTheCodeField() {
  openPage('')
  expect(await screen.findByRole('textbox', { name: 'Code from Setup' })).toBeInTheDocument()
  expect(screen.getByRole('button', { name: 'Find' })).toBeInTheDocument()
  expect(loadConnectRequest).not.toHaveBeenCalled()
}
it('shows the code field and Find when the address has no code', showsTheCodeField)

/** Checks that an empty code in the address also shows the field. */
async function emptyCodeShowsTheField() {
  openPage('?code=')
  expect(await screen.findByRole('textbox', { name: 'Code from Setup' })).toBeInTheDocument()
  expect(loadConnectRequest).not.toHaveBeenCalled()
}
it('shows the code field for an empty code in the address', emptyCodeShowsTheField)

/** Checks that the label sits above the field, not beside it. */
async function labelIsAboveTheField() {
  openPage('')
  const field = await screen.findByRole('textbox', { name: 'Code from Setup' })
  const label = screen.getByText('Code from Setup')
  const follows = label.compareDocumentPosition(field) & Node.DOCUMENT_POSITION_FOLLOWING
  expect(follows).toBeTruthy()
  expect(field).not.toHaveStyle({ textAlign: 'right' })
}
it('puts the label before the field and keeps the text left-aligned', labelIsAboveTheField)

/** Checks Find: the address changes and the request is read with the trimmed code. */
async function findGoesToTheCodeAddress() {
  const user = openPage('')
  await user.type(await screen.findByRole('textbox', { name: 'Code from Setup' }), '  abcd-efgh ')
  await user.click(screen.getByRole('button', { name: 'Find' }))
  expect(window.location.pathname + window.location.search).toBe('/connect?code=abcd-efgh')
  await waitFor(() => expect(loadConnectRequest).toHaveBeenCalledWith('abcd-efgh'))
  expect(await screen.findByText('PC-01')).toBeInTheDocument()
}
it('goes to /connect?code=<typed code> on Find and shows the request', findGoesToTheCodeAddress)

/** Checks that Find does nothing for a code of spaces only. */
async function findNeedsACode() {
  const user = openPage('')
  await user.type(await screen.findByRole('textbox', { name: 'Code from Setup' }), '   ')
  expect(screen.getByRole('button', { name: 'Find' })).toBeDisabled()
  await user.click(screen.getByRole('button', { name: 'Find' }))
  expect(window.location.search).toBe('')
}
it('keeps Find disabled while the code is empty', findNeedsACode)

/** Checks that the typed code is encoded in the address. */
async function findEncodesTheCode() {
  const user = openPage('')
  await user.type(await screen.findByRole('textbox', { name: 'Code from Setup' }), 'a&b=c')
  await user.click(screen.getByRole('button', { name: 'Find' }))
  expect(window.location.search).toBe('?code=a%26b%3Dc')
}
it('encodes the typed code in the address', findEncodesTheCode)
