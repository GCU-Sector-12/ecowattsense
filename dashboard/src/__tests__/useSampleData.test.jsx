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
    expect(result.current).toEqual({ data: null, error: null })
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
    expect(result.current).toEqual({ data: 'new', error: null })
    const pending = deferred()
    rerender({ loader: () => pending.promise })
    expect(result.current).toEqual({ data: null, error: null })
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
    expect(result.current).toEqual({ data: null, error: null })
  })
})
