// Provider lifecycle for data pages. Use a stable loader and call retry after an error.
// Late results are ignored so navigation and retries cannot restore an old response.
import { useCallback, useEffect, useState } from 'react'

/**
 * Loads data and exposes a retry action without remounting the page.
 * @template T
 * @param {() => Promise<T>} loader - Stable provider; synchronous errors are also handled.
 * @returns {{data: T|null, error: Error|null, retry: Function}} Null data means loading.
 */
export default function useSampleData(loader) {
  const [result, setResult] = useState({ data: null, error: null })
  const [attempt, setAttempt] = useState(0)
  const retry = useCallback(() => setAttempt(incrementAttempt), [])
  useEffect(() => startRequest(loader, setResult), [loader, attempt])
  return { ...result, retry }
}

/**
 * Advances the retry counter so the effect starts a new request.
 * @param {number} attempt - Previous attempt.
 * @returns {number} Next attempt.
 */
function incrementAttempt(attempt) {
  return attempt + 1
}

/**
 * Starts one request and returns its effect cleanup.
 * @param {Function} loader - Provider returning a value or promise.
 * @param {Function} setResult - React result setter.
 * @returns {Function} Cleanup that ignores both late success and late failure.
 */
function startRequest(loader, setResult) {
  let cancelled = false
  /** @param {*} data - Loaded value. @returns {void} Publishes only the active request. */
  function succeed(data) {
    if (!cancelled) setResult({ data, error: null })
  }
  /** @param {Error} error - Provider failure. @returns {void} Publishes only the active request. */
  function fail(error) {
    if (!cancelled) setResult({ data: null, error })
  }
  /** @returns {void} Stops this request from publishing after cleanup. */
  function cancel() {
    cancelled = true
  }
  setResult({ data: null, error: null })
  Promise.resolve().then(loader).then(succeed, fail)
  return cancel
}
