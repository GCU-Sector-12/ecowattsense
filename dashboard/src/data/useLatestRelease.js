// Reads the latest Windows release from the manifest on GitHub and the version installed on this computer.
// Used by components/InstallerBlock.jsx, which shows the version, the button and the menu.
import { useEffect, useState } from 'react'
import {
  INSTALLER_DOWNLOAD_URL,
  LATEST_RELEASE_MANIFEST_URL,
  LOCAL_CLIENT_TIMEOUT_MS,
  LOCAL_CLIENT_VERSION_URL,
  RELEASE_DOWNLOAD_PREFIX,
} from './ui.js'

const FALLBACK_STATE = { version: null, url: INSTALLER_DOWNLOAD_URL, localVersion: null }
const INSTALLER_FILE_NAME = 'EcoWattSense-Web-Setup.exe'
const VERSION_PATTERN = /^\d+(\.\d+)*$/

/**
 * Checks a version string that came from the network.
 * @param {*} value - Candidate version.
 * @returns {boolean} True for digits separated by dots.
 */
function isVersion(value) {
  return typeof value === 'string' && VERSION_PATTERN.test(value)
}

/**
 * Checks the manifest and builds the release. The data comes from the network, so it is untrusted.
 * @param {*} manifest - Parsed JSON of latest.json.
 * @returns {{version: string, url: string}|null} The release, or null when the data is not valid.
 */
export function releaseFromManifest(manifest) {
  const version = manifest?.version
  const zipUrl = manifest?.zip_url
  const zipOk = typeof zipUrl === 'string' && zipUrl.startsWith(RELEASE_DOWNLOAD_PREFIX)
  if (!isVersion(version) || !zipOk) return null
  const folder = zipUrl.slice(0, zipUrl.lastIndexOf('/') + 1)
  return { version, url: folder + INSTALLER_FILE_NAME }
}

/**
 * Compares two versions part by part as numbers; a missing part counts as 0.
 * @param {string} first - Version such as "0.1.10".
 * @param {string} second - Version such as "0.1.9".
 * @returns {number} Negative when first is lower, 0 when equal, positive when first is higher.
 */
export function compareVersions(first, second) {
  const a = first.split('.').map(Number)
  const b = second.split('.').map(Number)
  const length = Math.max(a.length, b.length)
  const differences = Array.from({ length }, (_, i) => (a[i] ?? 0) - (b[i] ?? 0))
  return differences.find(Boolean) ?? 0
}

/**
 * Decides what the block offers.
 * @param {string|null} latest - Latest version, or null when unknown.
 * @param {string|null} local - Version running on this computer, or null when none answered.
 * @returns {'install'|'update'|'current'} The state of the block.
 */
export function installState(latest, local) {
  if (!latest || !local) return 'install'
  return compareVersions(local, latest) < 0 ? 'update' : 'current'
}

/**
 * Turns the manifest response into a release. Rejects when the body is not JSON.
 * @param {Response} response - Answer of the manifest request.
 * @returns {Promise<{version: string, url: string}|null>} The release, or null.
 */
async function releaseFromResponse(response) {
  if (!response.ok) return null
  return releaseFromManifest(await response.json())
}

/**
 * Turns the local client response into a version. Rejects when the body is not JSON.
 * @param {Response} response - Answer of the local client.
 * @returns {Promise<string|null>} The version, or null.
 */
async function versionFromResponse(response) {
  if (!response.ok) return null
  const body = await response.json()
  return isVersion(body?.version) ? body.version : null
}

/**
 * Fetches the manifest once. Any failure (network, bad status, bad JSON) gives null.
 * @returns {Promise<{version: string, url: string}|null>} The release, or null.
 */
export async function loadLatestRelease() {
  try {
    return await releaseFromResponse(await fetch(LATEST_RELEASE_MANIFEST_URL))
  } catch {
    return null
  }
}

/**
 * Asks the client on this computer for its version. No credentials are sent; a slow or
 * missing client (the normal case when it is not installed) gives null after the timeout.
 * @returns {Promise<string|null>} The installed version, or null.
 */
export async function loadLocalVersion() {
  try {
    const response = await fetch(LOCAL_CLIENT_VERSION_URL, {
      credentials: 'omit',
      signal: AbortSignal.timeout(LOCAL_CLIENT_TIMEOUT_MS),
    })
    return await versionFromResponse(response)
  } catch {
    return null
  }
}

/**
 * Reads the manifest, then (only when it was valid) the local client.
 * @returns {Promise<{version: string|null, url: string, localVersion: string|null}>} Latest release and local version.
 */
async function loadState() {
  const release = await loadLatestRelease()
  if (!release) return FALLBACK_STATE
  const localVersion = await loadLocalVersion()
  return { ...release, localVersion }
}

/**
 * Loads the state once per mount, without polling.
 * @returns {{version: string|null, url: string, localVersion: string|null}} The fallback until the data arrives.
 */
export function useLatestRelease() {
  const [state, setState] = useState(FALLBACK_STATE)
  useEffect(() => {
    loadState().then(setState)
  }, [])
  return state
}
