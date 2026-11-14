// Presentation-only content; not part of the server API.
export const navigation = [
  { key: 'dashboard', label: 'Dashboard', icon: 'home' },
  { key: 'computers', label: 'Computers', icon: 'monitor' },
  { key: 'energy', label: 'Energy Usage', icon: 'bar-chart' },
  { key: 'settings', label: 'Settings', icon: 'settings' },
]

export const stateDisplay = {
  active: { label: 'Active Computers', caption: 'Actively being used' },
  idle: { label: 'Idle Computers', caption: 'No activity for 10+ mins' },
  unused: { label: 'Unused Computers', caption: 'No activity for 1+ hour' },
  no_data: { label: 'No Data', caption: 'No recent reports' },
}

// Demo-only credentials for EWS_WEB-33. These are not API authentication details.
export const sampleCredentials = {
  username: 'admin',
  password: 'admin12345',
}

// Fixed display context for the sample screens; neither field is API data.
export const sampleScreen = {
  generated_at: '2025-03-10T14:32:00Z',
  user_name: 'Admin',
}

export const stateLabels = {
  active: 'Active',
  idle: 'Idle',
  unused: 'Unused',
  no_data: 'No data',
}

export const stateColors = {
  active: 'var(--ews-active)',
  idle: 'var(--ews-idle)',
  unused: 'var(--ews-unused)',
  no_data: 'var(--ews-text-muted)',
}

// Fallback download address of the Windows installer, used when the latest release cannot be read.
export const INSTALLER_DOWNLOAD_URL =
  'https://github.com/GCU-Sector-12/ecowattsense/releases/download/v0.1.2-energy-suite/EcoWattSense-Web-Setup.exe'

// Manifest that names the latest Windows release (version and zip address).
export const LATEST_RELEASE_MANIFEST_URL =
  'https://raw.githubusercontent.com/GCU-Sector-12/ecowattsense/EWS-energy-suite/client/spike/energy-suite/release/latest.json'

// Every release download address must start with this prefix.
export const RELEASE_DOWNLOAD_PREFIX = 'https://github.com/GCU-Sector-12/ecowattsense/releases/download/'

// The EcoWattSense client on this computer answers its installed version here (JSON: {"version":"0.1.3"}).
export const LOCAL_CLIENT_VERSION_URL = 'http://127.0.0.1:47613/version'

// How long the dashboard waits for the local client before it treats it as not installed.
export const LOCAL_CLIENT_TIMEOUT_MS = 1500
