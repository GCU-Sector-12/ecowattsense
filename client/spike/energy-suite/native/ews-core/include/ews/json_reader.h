// json_reader.h: a small JSON reader for the calibration model file (SPEC 8).
// Used by the model parser in ews-service and by the tests. It reads text that is already in memory;
// it never touches a file, never throws, and gives a value tree or an error status.
// Limits: text at most 64 KB, containers nested at most 16 deep. Duplicate keys: the last one wins.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "ews/reading.h"

namespace ews {

/// The kind of a JSON value.
enum class JsonType { Null, Bool, Number, String, Array, Object };

/// One JSON value. Only the fields of its type are meaningful.
struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolean = false;            ///< Bool.
    double number = 0;               ///< Number (always finite).
    std::string text;                ///< String, as UTF-8 with the escapes resolved.
    std::vector<std::string> keys;   ///< Object: the keys, in file order (items[i] belongs to keys[i]).
    std::vector<JsonValue> items;    ///< Array: the elements. Object: the member values.

    /// Finds a member of an object.
    /// @param key The member name.
    /// @return The member value (the last one when the key repeats), or null when this is not an object or the key is missing.
    const JsonValue* Find(const std::string& key) const;
};

/// The largest text the reader accepts, in bytes.
constexpr std::size_t kJsonMaxTextBytes = 64 * 1024;

/// The deepest nesting of arrays and objects the reader accepts.
constexpr int kJsonMaxDepth = 16;

/// Reads one JSON document.
/// @param text The whole document. Only white space may follow the value.
/// @return Ok with the value tree, or Error (ERROR_INVALID_DATA) for empty text, bad syntax, trailing text,
///         text longer than 64 KB, nesting deeper than 16, or a number that does not fit a double.
Reading<JsonValue> ParseJson(std::string_view text);

}  // namespace ews
