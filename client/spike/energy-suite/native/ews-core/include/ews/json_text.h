// json_text.h: small text helpers for the writers of JSON and CSV text (string quoting, fixed decimals, UTC time).
// Used by the report, history and status writers, and by the scan output writer (the quoting was shared from there).
// All numbers use a dot and no locale.
#pragma once

#include <cstdint>
#include <string>

namespace ews {

/// Quotes text as a JSON string: quote and backslash get a backslash, control characters become \u00XX.
/// Bytes that are not valid UTF-8 become '?', so the output always parses.
/// @param text The UTF-8 text.
/// @return The text in double quotes.
std::string JsonQuote(const std::string& text);

/// Formats a finite number with a fixed count of decimals and a dot: FormatFixed(4.1, 3) is "4.100".
/// @param value A finite number.
/// @param decimals The number of decimals, 0 to 20.
/// @return The text, or an empty string when the conversion fails.
std::string FormatFixed(double value, int decimals);

/// Formats a Unix time as UTC text like 2026-11-14T10:05:00Z (whole seconds).
/// @param unixSeconds Seconds since 1970-01-01T00:00:00Z. A value below 0 or beyond year 9999 is clamped.
/// @return The 20-character text.
std::string FormatUtcTime(std::int64_t unixSeconds);

}  // namespace ews
