// Presentation-only content; not part of the server API.
export const navigation = [
  { key: 'dashboard', label: 'Dashboard', icon: 'home' },
  { key: 'computers', label: 'Computers', icon: 'monitor' },
  { key: 'energy', label: 'Energy Usage', icon: 'bar-chart' },
  { key: 'settings', label: 'Settings', icon: 'settings' },
]

// TODO EWS_WEB-26: derive Idle/Unused captions from getSettings() values
// idle_threshold_seconds and unused_threshold_seconds instead of these sample defaults.
export const stateDisplay = {
  active: { label: 'Active Computers', caption: 'Actively being used' },
  idle: { label: 'Idle Computers', caption: 'No activity for 10+ mins' },
  unused: { label: 'Unused Computers', caption: 'No activity for 1+ hour' },
  no_data: { label: 'No Data', caption: 'No recent reports' },
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
