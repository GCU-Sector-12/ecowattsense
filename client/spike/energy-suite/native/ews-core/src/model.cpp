// model.cpp: the calibration model parser (see ews/model.h and SPEC section 8).
// Used by ews-service. The file comes from a user account, so every field is checked and a single wrong
// field makes the whole model invalid.
#include "ews/model.h"

#include <algorithm>
#include <cmath>

#include <windows.h>

#include "ews/json_reader.h"

namespace ews {

namespace {

constexpr double kMaxWatts = 500.0;
constexpr double kMaxWattsPerPercent = 5.0;
constexpr std::size_t kUtcTextLength = 20;

/// @return The failure for an invalid model.
Reading<CalibrationModel> Invalid() { return Reading<CalibrationModel>::Failure(Status::Error, ERROR_INVALID_DATA); }

/// @return The member of the object when it exists and has this type, else null.
const JsonValue* FindOfType(const JsonValue& object, const char* key, JsonType type) {
    const JsonValue* found = object.Find(key);
    if (found == nullptr || found->type != type) {
        return nullptr;
    }
    return found;
}

/// Reads a number member and checks it is finite and within 0 to `maxValue`.
/// @return True when the member is valid; `out` then holds the value.
bool ReadBounded(const JsonValue& object, const char* key, double maxValue, double& out) {
    const JsonValue* found = FindOfType(object, key, JsonType::Number);
    if (found == nullptr || !std::isfinite(found->number)) {
        return false;
    }
    if (found->number < 0.0 || found->number > maxValue) {
        return false;
    }
    out = found->number;
    return true;
}

/// @return True when `schema` is the number 2.
bool SchemaIsTwo(const JsonValue& root) {
    const JsonValue* schema = FindOfType(root, "schema", JsonType::Number);
    return schema != nullptr && schema->number == 2.0;
}

/// @return True when a pattern character accepts a text character: 'd' is a digit, any other is itself.
bool MatchesPatternChar(char pattern, char c) {
    if (pattern == 'd') {
        return c >= '0' && c <= '9';
    }
    return pattern == c;
}

/// @return True when the text looks like YYYY-MM-DDTHH:MM:SSZ.
bool LooksLikeUtcTime(const std::string& text) {
    static const std::string kPattern = "dddd-dd-ddTdd:dd:ddZ";
    if (text.size() != kUtcTextLength) {
        return false;
    }
    return std::equal(kPattern.begin(), kPattern.end(), text.begin(), MatchesPatternChar);
}

/// Reads generated_utc.
bool ReadGeneratedUtc(const JsonValue& root, CalibrationModel& model) {
    const JsonValue* found = FindOfType(root, "generated_utc", JsonType::String);
    if (found == nullptr || !LooksLikeUtcTime(found->text)) {
        return false;
    }
    model.generatedUtc = found->text;
    return true;
}

/// @return True when the element is the string "cpu_package" or "dram".
bool IsKnownPart(const JsonValue& item) {
    return item.type == JsonType::String && (item.text == "cpu_package" || item.text == "dram");
}

/// @return True when the element is the string "cpu_package".
bool IsPackagePart(const JsonValue& item) { return item.type == JsonType::String && item.text == "cpu_package"; }

/// @return True when the element is the string "dram".
bool IsDramPart(const JsonValue& item) { return item.type == JsonType::String && item.text == "dram"; }

/// Reads measured_parts: an array that holds only the two known kinds.
bool ReadMeasuredParts(const JsonValue& root, CalibrationModel& model) {
    const JsonValue* parts = FindOfType(root, "measured_parts", JsonType::Array);
    if (parts == nullptr || !std::all_of(parts->items.begin(), parts->items.end(), IsKnownPart)) {
        return false;
    }
    model.measuresCpuPackage = std::any_of(parts->items.begin(), parts->items.end(), IsPackagePart);
    model.measuresDram = std::any_of(parts->items.begin(), parts->items.end(), IsDramPart);
    return true;
}

/// Reads the seven numbers of the "model" object.
bool ReadModelNumbers(const JsonValue& object, CalibrationModel& model) {
    return ReadBounded(object, "base_w", kMaxWatts, model.baseW) &&
           ReadBounded(object, "rest_w", kMaxWatts, model.restW) &&
           ReadBounded(object, "rest_w_per_cpu_pct", kMaxWattsPerPercent, model.restWPerCpuPct) &&
           ReadBounded(object, "cpu_w_per_pct", kMaxWattsPerPercent, model.cpuWPerPct) &&
           ReadBounded(object, "screen_w_at_50", kMaxWatts, model.screenWAt50) &&
           ReadBounded(object, "screen_w_per_pct", kMaxWattsPerPercent, model.screenWPerPct) &&
           ReadBounded(object, "graphics_w", kMaxWatts, model.graphicsW);
}

/// Checks every field of the document and fills the model.
bool ReadModel(const JsonValue& root, CalibrationModel& model) {
    if (root.type != JsonType::Object || !SchemaIsTwo(root)) {
        return false;
    }
    if (!ReadGeneratedUtc(root, model) || !ReadMeasuredParts(root, model)) {
        return false;
    }
    const JsonValue* numbers = FindOfType(root, "model", JsonType::Object);
    return numbers != nullptr && ReadModelNumbers(*numbers, model);
}

}  // namespace

Reading<CalibrationModel> ParseModelJson(std::string_view text) {
    Reading<JsonValue> parsed = ParseJson(text);
    if (!parsed.ok()) {
        return Invalid();
    }
    CalibrationModel model;
    if (!ReadModel(parsed.value, model)) {
        return Invalid();
    }
    return Reading<CalibrationModel>::Success(std::move(model));
}

}  // namespace ews
