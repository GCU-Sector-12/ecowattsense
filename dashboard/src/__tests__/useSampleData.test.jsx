// Tests shared sample loading, failure handling and lifecycle race protection.
import { act, renderHook, waitFor } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'
import useSampleData from '../useSampleData.js'

/** @returns {{promise: Promise, resolve: Function, reject: Function}} A controllable request. */
function deferred() {
  let resolve, reject
  const promise = new Promise((res, rej) => { resolve = res; reject = rej })
  return { promise, resolve, reject }
}

describe('useSampleData', () => {
  it('starts empty and publishes the resolved provider result', async () => {
    const request = deferred()
    const loader = vi.fn(() => request.promise)
    const { result, rerender } = renderHook(() => useSampleData(loader))
    expect(result.current).toMatchObject({ data: null, error: null })
    await act(async () => request.resolve({ devices: [] }))
    expect(result.current.data).toEqual({ devices: [] })
    rerender()
    expect(loader).toHaveBeenCalledTimes(1)
  })

  it('exposes rejected requests without an unhandled rejection', async () => {
    const error = new Error('Unavailable')
    const loader = () => Promise.reject(error)
    const { result } = renderHook(() => useSampleData(loader))
    await waitFor(() => expect(result.current.error).toBe(error))
    expect(result.current.data).toBeNull()
  })

  it('also handles a provider that throws synchronously', async () => {
    const error = new Error('Invalid provider')
    const loader = () => { throw error }
    const { result } = renderHook(() => useSampleData(loader))
    await waitFor(() => expect(result.current.error).toBe(error))
  })

  it('ignores a late result after switching loaders and clears previous data', async () => {
    const old = deferred()
    const current = deferred()
    const firstLoader = () => old.promise
    const nextLoader = () => current.promise
    const { result, rerender } = renderHook(({ loader }) => useSampleData(loader), {
      initialProps: { loader: firstLoader },
    })
    await act(async () => {})
    rerender({ loader: nextLoader })
    await act(async () => current.resolve('new'))
    await act(async () => old.resolve('old'))
    expect(result.current).toMatchObject({ data: 'new', error: null })
    const pending = deferred()
    rerender({ loader: () => pending.promise })
    expect(result.current).toMatchObject({ data: null, error: null })
    await act(async () => pending.resolve('latest'))
    expect(result.current.data).toBe('latest')
  })

  it('ignores a late rejection after unmount', async () => {
    const request = deferred()
    const loader = () => request.promise
    const { result, unmount } = renderHook(() => useSampleData(loader))
    await act(async () => {})
    unmount()
    await act(async () => request.reject(new Error('Late failure')))
    expect(result.current).toMatchObject({ data: null, error: null })
  })
})

/** @returns {Promise<void>} Checks that retry clears the error and starts the same loader again. */
async function retriesFailure() {
  const loader = vi.fn().mockRejectedValueOnce(new Error('Unavailable')).mockResolvedValueOnce('recovered')
  const { result } = renderHook(() => useSampleData(loader))
  await waitFor(() => expect(result.current.error).toBeTruthy())
  const retry = result.current.retry
  act(retry)
  expect(result.current.error).toBeNull()
  await waitFor(() => expect(result.current.data).toBe('recovered'))
  expect(loader).toHaveBeenCalledTimes(2)
  expect(result.current.retry).toBe(retry)
}
it('retries a failed loader', retriesFailure)


/** @returns {void} Makes the document visibility state controllable in fake-timer tests. */
function setVisibility(state) {
  Object.defineProperty(document, 'visibilityState', { configurable: true, value: state })
}

/** @returns {Promise<void>} Checks visible pages refresh after 60 seconds without dropping old data. */
async function refreshesVisibleData() {
  vi.useFakeTimers()
  setVisibility('visible')
  const next = deferred()
  const loader = vi.fn().mockResolvedValueOnce('first').mockImplementationOnce(() => next.promise)
  const { result } = renderHook(() => useSampleData(loader))
  await act(async () => {})
  expect(result.current.data).toBe('first')
  await act(async () => vi.advanceTimersByTimeAsync(60000))
  expect(loader).toHaveBeenCalledTimes(2)
  expect(result.current.data).toBe('first')
  expect(result.current.loading).toBe(false)
  await act(async () => next.resolve('second'))
  expect(result.current.data).toBe('second')
  expect(result.current.lastUpdatedAt).toBeInstanceOf(Date)
}
it('refreshes visible data every 60 seconds without clearing the current result', refreshesVisibleData)

/** @returns {Promise<void>} Checks hidden tabs pause polling and refresh as soon as they become visible. */
async function refreshesWhenTabReturns() {
  vi.useFakeTimers()
  setVisibility('hidden')
  const loader = vi.fn().mockResolvedValue('loaded')
  const { result } = renderHook(() => useSampleData(loader))
  await act(async () => {})
  await act(async () => vi.advanceTimersByTimeAsync(120000))
  expect(loader).toHaveBeenCalledTimes(1)
  setVisibility('visible')
  await act(async () => document.dispatchEvent(new Event('visibilitychange')))
  expect(loader).toHaveBeenCalledTimes(2)
  expect(result.current.data).toBe('loaded')
}
it('does not poll a hidden tab and refreshes immediately when it becomes visible', refreshesWhenTabReturns)

/** @returns {Promise<void>} Checks failed refreshes preserve the last result and retry on the next interval. */
async function keepsDataAfterRefreshFailure() {
  vi.useFakeTimers()
  setVisibility('visible')
  const failure = new Error('offline')
  const loader = vi.fn().mockResolvedValueOnce('current')
    .mockRejectedValueOnce(failure).mockResolvedValueOnce('fresh')
  const { result } = renderHook(() => useSampleData(loader))
  await act(async () => {})
  await act(async () => vi.advanceTimersByTimeAsync(60000))
  expect(result.current.data).toBe('current')
  expect(result.current.refreshError).toBe(failure)
  await act(async () => vi.advanceTimersByTimeAsync(60000))
  expect(result.current.data).toBe('fresh')
  expect(result.current.refreshError).toBeNull()
}
it('keeps old data after a failed background refresh and tries again', keepsDataAfterRefreshFailure)

/** @returns {Promise<void>} Checks pages with active forms can disable background refresh. */
async function pausesWhenDisabled() {
  vi.useFakeTimers()
  setVisibility('visible')
  const loader = vi.fn().mockResolvedValue('loaded')
  const { result } = renderHook(() => useSampleData(loader, { enabled: false }))
  await act(async () => {})
  await act(async () => vi.advanceTimersByTimeAsync(120000))
  expect(loader).toHaveBeenCalledTimes(1)
  expect(result.current.data).toBe('loaded')
}
it('does not refresh while a page form or edit dialog is active', pausesWhenDisabled)
