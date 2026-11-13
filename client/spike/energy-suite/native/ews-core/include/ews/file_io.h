// file_io.h: small file helpers shared by the file parts of ews-core (config, queue, identity, history, log).
// Used by those parts only. Every function takes full paths or the data folder (`<data>` of SPEC 2.2).
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "ews/reading.h"

namespace ews {

/// Reads a whole file. The file is opened with full sharing, so a writer or a rename in progress does not block it.
/// @param path The file.
/// @param maxBytes The largest accepted size. A larger file is not read.
/// @return Ok with the bytes. A missing file gives Unavailable (error 2 or 3). A file over `maxBytes` gives Error
///         with ERROR_FILE_TOO_LARGE. Any other failure gives its status and Windows error code.
Reading<std::string> ReadWholeFile(const std::filesystem::path& path, std::size_t maxBytes);

/// Makes sure the folder `<data>\<name>` exists. Only one level is created: when `<data>` itself is missing this fails,
/// so a wrong data folder never creates a tree.
/// @param dataDir The data folder.
/// @param name The sub folder name, without separators.
/// @return Ok with true when the folder exists afterwards, otherwise the failure with the Windows error code.
Reading<bool> EnsureSubfolder(const std::wstring& dataDir, const std::wstring& name);

/// Renames a file over another one (an existing target is replaced).
/// @param from The existing file.
/// @param to The new name.
/// @return Ok with true, or the failure with the Windows error code (a sharing violation when `to` is open elsewhere).
Reading<bool> ReplaceByRename(const std::filesystem::path& from, const std::filesystem::path& to);

}  // namespace ews
