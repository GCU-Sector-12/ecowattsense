// Shared data loader for dashboard pages. Keeps old results during automatic refreshes.
import { useCallback, useEffect, useRef, useState } from 'react'

const REFRESH_INTERVAL = 60000

/**
 * Loads page data and refreshes it while the tab is visible.
 * @param {() => Promise<*>} loader - Stable provider for the current page and selection.
 * @param {{enabled?: boolean}} [options] - Disable polling while a form or dialog is active.
 * @returns {{data: *, error: Error|null, refreshError: Error|null, lastUpdatedAt: Date|null, loading: boolean, retry: Function}} Request state and retry action.
 */
export default function useSampleData(loader, options = {}) {
  const [result, setResult] = useState(initialResult)
  const [attempt, setAttempt] = useState(0)
  const requestRef = useRef(null)
  const enabled = options.enabled !== false
  const hasData = result.data !== null
  const retry = useCallback(() => setAttempt(incrementAttempt), [])

  useEffect(() => startRequest(loader, setResult, requestRef), [loader, attempt])

  useEffect(() => {
    if (!enabled || !hasData) return undefined
    const scope = { active: true }
    const interval = window.setInterval(() => refreshIfVisible(loader, setResult, requestRef, scope), REFRESH_INTERVAL)
    scope.onVisibilityChange = () => refreshWhenVisible(loader, setResult, requestRef, scope)
    document.addEventListener('visibilitychange', scope.onVisibilityChange)
    return () => stopRefreshing(interval, scope)
  }, [loader, enabled, hasData])

  return { ...result, retry }
}

/** @returns {object} Empty request state before the first response. */
function initialResult() {
  return { data: null, error: null, refreshError: null, lastUpdatedAt: null, loading: true }
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
 * Starts the initial request and ignores its result after the effect is cleaned up.
 * @param {Function} loader - Provider returning a value or promise.
 * @param {Function} setResult - React result setter.
 * @param {{current: object|null}} requestRef - Current in-flight request token.
 * @returns {Function} Cleanup that prevents a late result from publishing.
 */
function startRequest(loader, setResult, requestRef) {
  const token = { cancelled: false }
  requestRef.current = token
  setResult(initialResult())
  Promise.resolve().then(loader)
    .then(data => publishInitialData(data, token, setResult), error => publishInitialFailure(error, token, setResult))
    .finally(() => releaseRequest(requestRef, token))
  return () => cancelRequest(token)
}

/** @param {*} data - Initial provider result. @param {object} token - Request lifecycle token. @param {Function} setResult - React result setter. @returns {void} Publishes an active response. */
function publishInitialData(data, token, setResult) {
  if (token.cancelled) return
  setResult({ data, error: null, refreshError: null, lastUpdatedAt: new Date(), loading: false })
}

/** @param {Error} error - Initial provider failure. @param {object} token - Request lifecycle token. @param {Function} setResult - React result setter. @returns {void} Publishes an active failure. */
function publishInitialFailure(error, token, setResult) {
  if (token.cancelled) return
  setResult({ data: null, error, refreshError: null, lastUpdatedAt: null, loading: false })
}

/** @param {object} token - Request lifecycle token. @returns {void} Stops a cleaned-up request from publishing. */
function cancelRequest(token) {
  token.cancelled = true
}

/** @param {{current: object|null}} requestRef - Current request token. @param {object} token - Finished request token. @returns {void} Clears only the matching request. */
function releaseRequest(requestRef, token) {
  if (requestRef.current === token) requestRef.current = null
}

/**
 * Refreshes the current data without replacing it with a loading state.
 * @param {Function} loader - Provider for the current page selection.
 * @param {Function} setResult - React result setter.
 * @param {{current: object|null}} requestRef - Current in-flight request token.
 * @param {{active: boolean}} scope - Lifetime of the polling effect.
 * @returns {void}
 */
function refreshIfVisible(loader, setResult, requestRef, scope) {
  if (!scope.active || !isTabVisible() || requestRef.current) return
  const token = { cancelled: false }
  requestRef.current = token
  Promise.resolve().then(loader)
    .then(data => publishRefresh(data, token, scope, setResult), error => publishRefreshFailure(error, token, scope, setResult))
    .finally(() => releaseRequest(requestRef, token))
}

/** @param {*} data - Fresh provider result. @param {object} token - Request lifecycle token. @param {{active: boolean}} scope - Polling effect lifetime. @param {Function} setResult - React result setter. @returns {void} Replaces data only after a successful response. */
function publishRefresh(data, token, scope, setResult) {
  if (token.cancelled || !scope.active) return
  setResult({ data, error: null, refreshError: null, lastUpdatedAt: new Date(), loading: false })
}

/** @param {Error} error - Background provider failure. @param {object} token - Request lifecycle token. @param {{active: boolean}} scope - Polling effect lifetime. @param {Function} setResult - React result setter. @returns {void} Keeps the last successful data and records the failure. */
function publishRefreshFailure(error, token, scope, setResult) {
  if (token.cancelled || !scope.active) return
  setResult(current => ({ ...current, refreshError: error }))
}

/** @param {Function} loader - Provider for the current page selection. @param {Function} setResult - React result setter. @param {{current: object|null}} requestRef - Current request token. @param {{active: boolean}} scope - Polling effect lifetime. @returns {void} Refreshes immediately when the tab becomes visible. */
function refreshWhenVisible(loader, setResult, requestRef, scope) {
  if (!isTabVisible()) return
  refreshIfVisible(loader, setResult, requestRef, scope)
}

/** @returns {boolean} True unless the browser has hidden this tab. */
function isTabVisible() {
  return document.visibilityState !== 'hidden'
}

/** @param {number} interval - Polling interval handle. @param {{active: boolean}} scope - Polling effect lifetime. @returns {void} Stops polling and removes its event listener. */
function stopRefreshing(interval, scope) {
  scope.active = false
  window.clearInterval(interval)
  document.removeEventListener('visibilitychange', scope.onVisibilityChange)
}
