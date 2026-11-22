// configure.h: the `--configure` input of the service (SPEC 5.2): lines `url=<...>` and `key=<...>`.
// Used by ews-service when it runs with `--configure`. The text comes from standard input, so it is untrusted.
// The key travels only through standard input, never through a command line, and is never echoed.
#pragma once

#include <cstddef>
#include <string>

namespace ews {

/// The largest accepted input.
inline constexpr std::size_t kMaxConfigureInputBytes = 4 * 1024;

/// The fixed answers of the configure input.
enum class ConfigureResult {
    Ok,           ///< Everything that was given has been written.
    BadUrl,       ///< The URL fails IsValidServerUrl, or the input has neither a `url=` nor a `key=` line.
    BadKey,       ///< The key fails IsValidServerKey.
    TooLong,      ///< The input is over 4 KB.
    WriteFailed   ///< service.ini or key.bin could not be written.
};

/// @param result A result.
/// @return The fixed word for it: `ok`, `bad_url`, `bad_key`, `too_long` or `write_failed`.
const char* ConfigureResultWord(ConfigureResult result);

/// Checks the input and writes the URL into service.ini (other keys stay) and the key into key.bin.
/// Both are checked before anything is written. A missing `url=` line keeps the stored URL; a missing `key=` line
/// keeps the stored key. A line that starts with neither `url=` nor `key=` is ignored. When a line appears twice the
/// last one counts. Only a final carriage return is cut from a line; a space at the end of a value is part of it.
/// The key is written first, then the URL; when the second write fails the first stays.
/// @param input The text read from standard input.
/// @param dataDir The data folder.
/// @return A fixed result. The input is never copied into the result.
ConfigureResult ApplyConfigureInput(const std::string& input, const std::wstring& dataDir);

}  // namespace ews
