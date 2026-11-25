// Tests safe developer mode preference access when browser storage is restricted.
import { afterEach, beforeEach, expect, it } from 'vitest'
import { readDeveloperMode, saveDeveloperMode } from '../data/developerMode.js'

let originalStorageDescriptor

/**
 * Replaces localStorage with a getter that simulates browser storage restrictions.
 * @returns {void}
 */
function blockStorage() {
  originalStorageDescriptor = Object.getOwnPropertyDescriptor(window, 'localStorage')
  Object.defineProperty(window, 'localStorage', {
    configurable: true,
    get() {
      throw new Error('Storage access is blocked')
    },
  })
}

/**
 * Restores the browser's original localStorage property.
 * @returns {void}
 */
function restoreStorage() {
  if (originalStorageDescriptor) {
    Object.defineProperty(window, 'localStorage', originalStorageDescriptor)
    return
  }
  delete window.localStorage
}

beforeEach(blockStorage)
afterEach(restoreStorage)

/**
 * Checks blocked browser storage cannot prevent pages from rendering or toggles from failing.
 * @returns {void}
 */
function handlesBlockedStorage() {
  expect(readDeveloperMode()).toBe(false)
  expect(saveDeveloperMode(true)).toBe(false)
  expect(saveDeveloperMode(false)).toBe(false)
  expect(readDeveloperMode()).toBe(false)
}
it('uses the default preference and fails safely when localStorage is blocked', handlesBlockedStorage)
