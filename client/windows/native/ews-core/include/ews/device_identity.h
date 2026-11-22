// device_identity.h: the device id in `<data>\Client\device-id.txt` (SPEC 5.4), shared with the old client.
// Used by ews-service at start. The file holds one GUID in "D" format and a line feed (the format of the
// DeviceIdentityStore of the client). A file that exists but is bad is an error and is never replaced.
#pragma once

#include <string>

#include "ews/reading.h"

namespace ews {

/// The device id.
struct DeviceIdentity {
    std::string id;        ///< Lower case, with hyphens (`xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`).
    bool created = false;  ///< True when this call made the file.
};

/// Checks and normalises a device id text. Spaces and line ends around the GUID are allowed.
/// @param text The text of the file.
/// @param id Receives the lower case GUID when the text is valid.
/// @return True for one GUID in "D" format that is not all zero.
bool ParseDeviceId(const std::string& text, std::string& id);

/// Reads the identity, or creates it when the file is confirmed missing. The new file is written to a temporary
/// file in the same folder and renamed without replacing, so it never shows half a GUID and never overwrites an
/// identity made at the same moment by the client. When another process wins that race its identity is read.
/// @param dataDir The data folder; the `Client` sub folder is created when missing.
/// @return Ok with the identity. Error (ERROR_INVALID_DATA) when the file exists but is bad or over 256 bytes;
///         the file is left as it is. Another failure gives its status and Windows error code.
Reading<DeviceIdentity> LoadOrCreateDeviceIdentity(const std::wstring& dataDir);

}  // namespace ews
