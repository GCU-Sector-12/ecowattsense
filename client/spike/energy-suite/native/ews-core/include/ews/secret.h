// secret.h: keeps the server key protected on disk with DPAPI (SPEC 5.6).
// Used by ews-service (reads the key before a send) and by the configure input (writes it).
// The key is protected with the machine scope and no user interface, and stored as `<data>\config\key.bin`.
// A key.bin made on another computer cannot be opened; that is an Error, never a crash.
#pragma once

#include <cstddef>
#include <string>

#include "ews/reading.h"

namespace ews {

/// The longest accepted server key.
inline constexpr std::size_t kMaxServerKeyLength = 256;

/// Checks a server key: 1 to 256 printable ASCII characters (0x20 to 0x7E), no space at either end.
/// @param key The text to check.
/// @return True when the key follows the rule.
bool IsValidServerKey(const std::string& key);

/// Protects bytes with DPAPI (CryptProtectData, machine scope, no user interface).
/// @param plain The bytes to protect.
/// @return Ok with the protected blob, or the failure with the Windows error code.
Reading<std::string> ProtectBytes(const std::string& plain);

/// Opens a blob made by ProtectBytes.
/// @param blob The protected bytes.
/// @return Ok with the plain bytes. A damaged blob, or one from another computer, gives Error.
Reading<std::string> UnprotectBytes(const std::string& blob);

/// Protects the key and writes `<data>\config\key.bin` atomically.
/// @param dataDir The data folder; the `config` sub folder is created when missing.
/// @param key A key that passed IsValidServerKey.
/// @return Ok with true, or the failure. The key is never logged or returned in text.
Reading<bool> StoreServerKey(const std::wstring& dataDir, const std::string& key);

/// Reads and opens `<data>\config\key.bin`.
/// @param dataDir The data folder.
/// @return Ok with the key. A missing file gives Unavailable. A damaged file, one from another computer, or one that
///         holds a key that fails IsValidServerKey gives Error.
Reading<std::string> LoadServerKey(const std::wstring& dataDir);

}  // namespace ews
