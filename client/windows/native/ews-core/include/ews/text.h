// text.h: small text helpers shared by the readers (UTF-16 to UTF-8).
// Used by the EMI, power meter and graphics readers, whose names come from drivers as UTF-16.
#pragma once

#include <cstddef>
#include <string>

namespace ews {

/// Converts UTF-16 text to UTF-8.
/// @param text The UTF-16 characters; may be null only when length is 0.
/// @param length The number of UTF-16 characters (not bytes). The text need not end with a terminator.
/// @return The UTF-8 text. An empty string for empty input or when Windows cannot convert it.
std::string WideToUtf8(const wchar_t* text, std::size_t length);

}  // namespace ews
