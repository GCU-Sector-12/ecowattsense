// atomic_file.h: writes a small file so a reader never sees half of it.
// Used by ews-scan (capabilities.json, scan.log) and, later, by ews-service (status.json).
// The data goes to a temporary file in the same folder; then that file is renamed over the target.
#pragma once

#include <string>

#include "ews/reading.h"

namespace ews {

/// Writes `bytes` to `<folder>\<name>` through a temporary file `<name>.tmp` in the same folder.
/// If any step fails, the temporary file is deleted and the old target (if any) stays as it was.
/// @param folder The folder; it must exist.
/// @param name The file name, without a folder.
/// @param bytes The content, written as it is (the caller makes it UTF-8).
/// @return Ok with true when the target now holds the content. Otherwise the failure with the Windows error code
///         (for example Unavailable when the folder is missing, Denied when it is not writable, Error when the
///         target is open in another program and the rename fails).
Reading<bool> WriteFileAtomically(const std::wstring& folder, const std::wstring& name, const std::string& bytes);

}  // namespace ews
