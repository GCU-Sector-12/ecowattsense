// Development request controls for data providers. Call before loading a response.
// URL switches let developers check slow requests and recovery without changing fixtures.

/**
 * Applies the current development-only delay and failure switches.
 * @returns {Promise<void>} Resolves when the simulated request may continue.
 * @throws {Error} When the development URL contains fail=1.
 */
export async function applyDevelopmentControls() {
  if (!import.meta.env.DEV) return
  const parameters = new URLSearchParams(window.location.search)
  const delay = Number(parameters.get('delay'))
  await waitForDelay(delay)
  if (parameters.get('fail') === '1') throw new Error('Simulated data failure')
}

/**
 * Waits for a valid browser timer delay; invalid values leave requests unchanged.
 * @param {number} delay - Requested milliseconds.
 * @returns {Promise<void>|undefined} Timer completion, or no wait.
 */
function waitForDelay(delay) {
  if (!Number.isInteger(delay) || delay <= 0 || delay > 2147483647) return
  return new Promise(resolve => setTimeout(resolve, delay))
}
