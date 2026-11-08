// Shared sample-provider lifecycle for Dashboard, Computers and Settings pages.
// Call useSampleData with a stable loader to read its data and error result.
import { useEffect, useState } from 'react'

/**
 * Loads sample data and ignores results from unmounted or superseded requests.
 * Keep loader stable (module-level or useCallback) to avoid repeated requests.
 * @template T
 * @param {() => Promise<T>} loader - Provider to run when its identity changes.
 * @returns {{data: T|null, error: Error|null}} Pending data is null; failures are exposed to the page.
 */
export default function useSampleData(loader) {
  const [result, setResult] = useState({ data: null, error: null })
  useEffect(() => {
    let cancelled = false
    setResult({ data: null, error: null })
    Promise.resolve().then(loader).then(
      (data) => { if (!cancelled) setResult({ data, error: null }) },
      (error) => { if (!cancelled) setResult({ data: null, error }) },
    )
    return () => { cancelled = true }
  }, [loader])
  return result
}
