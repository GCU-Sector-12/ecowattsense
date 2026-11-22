// line_text.h: tiny text helpers for the line based files of the service (service.ini and the configure input).
// Used by config.cpp and configure.cpp.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ews {

/// Splits text into lines at line feeds. One carriage return at the end of a line is removed.
/// A last line without a line feed is kept. An empty text gives no lines.
/// @param text The text to split.
/// @return The lines, without their line ends.
std::vector<std::string> SplitLines(const std::string& text);

/// Removes spaces and tabs at both ends.
/// @param text The text.
/// @return The trimmed copy.
std::string TrimBlanks(const std::string& text);

/// Reads a whole number written with digits only (no sign, no spaces, one to nine digits).
/// @param text The text.
/// @return The number, or nothing when the text has anything else.
std::optional<int> ParseWholeNumber(const std::string& text);

}  // namespace ews
