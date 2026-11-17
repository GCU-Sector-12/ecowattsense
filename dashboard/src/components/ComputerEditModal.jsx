// Computer editor shared by the Computers table and computer detail page.
import { useEffect, useState } from 'react'
import { Alert, Button, Group, Modal, Select, Stack, Switch, TextInput } from '@mantine/core'
import { getLocations, saveDevice } from '../data/index.js'

/**
 * Edits a computer's name, room and archived state.
 * @param {object} props - Selected computer, modal state and save callback.
 * @returns {JSX.Element} Accessible computer edit dialog.
 */
export default function ComputerEditModal({ computer, opened, onClose, onSaved }) {
  const [name, setName] = useState('')
  const [locationId, setLocationId] = useState(null)
  const [archived, setArchived] = useState(false)
  const [locations, setLocations] = useState([])
  const [error, setError] = useState('')
  const [saving, setSaving] = useState(false)
  const [loadingLocations, setLoadingLocations] = useState(false)

  useEffect(() => {
    if (!opened || !computer) return
    setName(computer.display_name ?? '')
    setLocationId(computer.location_id === null ? null : String(computer.location_id))
    setArchived(Boolean(computer.archived))
    setError('')
    setLoadingLocations(true)
    getLocations().then(setLocationOptions).catch(showLocationError).finally(stopLocationLoading)
  }, [opened, computer])

  /**
   * Stores the room options in the shape Mantine Select expects.
   * @param {{locations: object[]}} response - Room list response.
   * @returns {void}
   */
  function setLocationOptions(response) {
    setLocations(response.locations.map(location => ({
      value: String(location.id),
      label: location.building ? location.name + ' — ' + location.building : location.name,
    })))
  }

  /** @param {Error} reason - Failed room request. @returns {void} Shows a useful message. */
  function showLocationError(reason) {
    setError(reason.status === 401 ? 'Your session has expired. Please log in again.' : 'Could not load rooms. You can still save the other fields.')
  }

  /** @returns {void} Clears the room loading state. */
  function stopLocationLoading() {
    setLoadingLocations(false)
  }

  /** @param {Event} event - Name input event. @returns {void} Stores the typed name. */
  function changeName(event) {
    setName(event.currentTarget.value)
  }

  /** @param {boolean} checked - New archive state. @returns {void} Stores the switch value. */
  function changeArchived(checked) {
    setArchived(checked)
  }

  /** @returns {object} Only fields whose values differ from the selected computer. */
  function changedFields() {
    const changes = {}
    const nextName = name.trim() || null
    if (nextName !== computer.display_name) changes.display_name = nextName
    const nextLocation = locationId === null ? null : Number(locationId)
    if (nextLocation !== computer.location_id) changes.location_id = nextLocation
    if (archived !== Boolean(computer.archived)) changes.archived = archived
    return changes
  }

  /**
   * Saves only changed fields and returns the updated device to the parent.
   * @returns {Promise<void>} Resolves after a successful save.
   */
  async function handleSave() {
    const changes = changedFields()
    if (!Object.keys(changes).length) {
      onClose()
      return
    }
    if (name.trim().length > 60) {
      setError('Computer name must be 60 characters or fewer.')
      return
    }
    setSaving(true)
    setError('')
    try {
      const updated = await saveDevice(computer.device_id, changes)
      onSaved(updated)
      onClose()
    } catch (reason) {
      setError(saveError(reason))
    } finally {
      setSaving(false)
    }
  }

  return (
    <Modal opened={opened} onClose={onClose} title="Edit computer" centered>
      <Stack>
        {error && <Alert color="red" role="alert">{error}</Alert>}
        <TextInput label="Computer name" value={name} onChange={changeName}
          maxLength={60} description="Leave blank to use the unnamed computer label." />
        <Select label="Location" placeholder="No location" clearable searchable
          data={locations} value={locationId} onChange={setLocationId}
          disabled={loadingLocations} nothingFoundMessage="No rooms available" />
        <Switch label="Archived" checked={archived} onChange={(event) => changeArchived(event.currentTarget.checked)} />
        <Group justify="flex-end">
          <Button variant="default" onClick={onClose} disabled={saving}>Cancel</Button>
          <Button onClick={handleSave} loading={saving}>Save changes</Button>
        </Group>
      </Stack>
    </Modal>
  )
}

/**
 * Converts known API failures into messages that explain the next action.
 * @param {Error} reason - Error with an optional HTTP status.
 * @returns {string} User-facing save error.
 */
function saveError(reason) {
  if (reason.status === 422) return 'The computer details were not accepted. Check the name and location and try again.'
  if (reason.status === 404) return 'This computer could not be found. Refresh the computer list and try again.'
  if (reason.status === 401) return 'Your session has expired. Please log in again.'
  return 'Could not save this computer. Check your connection and try again.'
}
