// server_sync.cpp: the pure parts of the ping, the server settings and the commands (see ews/server_sync.h).
// Used by ServiceCore and the tests. Every value is range-checked; a bad value makes the whole text unusable.
#include "ews/server_sync.h"

#include <algorithm>
#include <cmath>

#include "ews/json_reader.h"
#include "ews/json_text.h"
#include "ews/line_text.h"

namespace ews {

namespace {

constexpr std::int64_t kMaxWhole = 9007199254740991;  // 2^53 - 1: the largest whole number a double holds exactly.
constexpr std::size_t kMaxTypeChars = 40;

/// Reads a JSON value as a whole number in a range.
/// @param value The value, or null.
/// @param low The smallest accepted number.
/// @param high The largest accepted number.
/// @return The number, or nothing when the value is missing, not a number, not whole or out of range.
std::optional<std::int64_t> WholeIn(const JsonValue* value, std::int64_t low, std::int64_t high) {
    bool number = value != nullptr && value->type == JsonType::Number;
    bool whole = number && std::floor(value->number) == value->number;
    bool inRange = whole && value->number >= static_cast<double>(low) && value->number <= static_cast<double>(high);
    return inRange ? std::optional<std::int64_t>(static_cast<std::int64_t>(value->number)) : std::nullopt;
}

/// @param seconds A number of seconds.
/// @return The seconds rounded to whole minutes (half up).
int RoundToMinutes(std::int64_t seconds) { return static_cast<int>((seconds + 30) / 60 * 60); }

/// Reads the optional `last_settings_utc` of the file.
/// @param root The object of the file.
/// @return The Unix time, or nothing when the field is missing or not of the exact form.
std::optional<std::int64_t> CheckedTime(const JsonValue& root) {
    const JsonValue* text = root.Find("last_settings_utc");
    std::int64_t unix = 0;
    bool valid = text != nullptr && text->type == JsonType::String && ParseUtcTime(text->text, unix);
    return valid ? std::optional<std::int64_t>(unix) : std::nullopt;
}

/// Reads the four values and the version into settings.
/// @param root The object of the answer or of the file.
/// @param versionRequired True when a missing version is an error.
/// @param out Receives the values when the function returns true.
/// @return True when every value is present and in range and the unused threshold is above the idle threshold.
bool ReadSettingsValues(const JsonValue& root, bool versionRequired, ServerSettings& out) {
    std::optional<std::int64_t> version = WholeIn(root.Find("settings_version"), 0, kMaxWhole);
    std::optional<std::int64_t> report = WholeIn(root.Find("report_interval_seconds"), 60, 3600);
    std::optional<std::int64_t> ping = WholeIn(root.Find("ping_interval_seconds"), 1, 86400);
    std::optional<std::int64_t> idle = WholeIn(root.Find("idle_threshold_seconds"), 1, 86400);
    std::optional<std::int64_t> unused = WholeIn(root.Find("unused_threshold_seconds"), 1, 604800);
    bool present = report && ping && idle && unused && (version || !versionRequired);
    bool ordered = present && *unused > *idle;
    if (!ordered) {
        return false;
    }
    out.version = version;
    out.reportIntervalSeconds = RoundToMinutes(*report);
    out.pingIntervalSeconds = *NormalisePingInterval(*ping);
    out.idleThresholdSeconds = static_cast<int>(*idle);
    out.unusedThresholdSeconds = static_cast<int>(*unused);
    return true;
}

/// Reads one command of the list.
/// @param item One list item.
/// @return The command, or nothing when the id, the type or the time is bad.
std::optional<ServerCommand> ReadCommand(const JsonValue& item) {
    std::optional<std::int64_t> id = WholeIn(item.Find("id"), 1, kMaxWhole);
    const JsonValue* type = item.Find("type");
    const JsonValue* expires = item.Find("expires_at");
    bool typed = type != nullptr && type->type == JsonType::String && !type->text.empty() && type->text.size() <= kMaxTypeChars;
    bool timed = expires != nullptr && expires->type == JsonType::String;
    ServerCommand command;
    bool dated = timed && ParseUtcTime(expires->text, command.expiresUnix);
    if (!id || !typed || !dated) {
        return std::nullopt;
    }
    command.id = *id;
    command.type = type->text;
    const JsonValue* params = item.Find("params");
    command.intervalParam = params != nullptr ? WholeIn(params->Find("report_interval_seconds"), 1, kMaxWhole) : std::nullopt;
    return command;
}

/// Adds the command of one list item to the answer when it is valid.
void AddCommand(PingAnswer& answer, const JsonValue& item) {
    std::optional<ServerCommand> command = ReadCommand(item);
    if (command.has_value()) {
        answer.commands.push_back(*command);
    }
}

/// @return True when a command has a lower id than another (the order of the sort).
bool IdLess(const ServerCommand& left, const ServerCommand& right) { return left.id < right.id; }

/// @param text A result word.
/// @return The result, or Failed for any other text.
CommandResult ResultFromWord(const std::string& text) {
    if (text == "done") {
        return CommandResult::Done;
    }
    return text == "unsupported" ? CommandResult::Unsupported : CommandResult::Failed;
}

/// Adds one `{"id":..,"result":..}` item of commands-done.json to the list when it is valid.
void AddDone(std::vector<DoneCommand>& done, const JsonValue& item) {
    std::optional<std::int64_t> id = WholeIn(item.Find("id"), 1, kMaxWhole);
    const JsonValue* result = item.Find("result");
    bool texted = result != nullptr && result->type == JsonType::String;
    if (!id || !texted) {
        return;
    }
    done.push_back(DoneCommand{*id, ResultFromWord(result->text)});
}

/// @param listed Commands of an answer.
/// @param id A command id.
/// @return True when one of the commands has the id.
bool IsListed(const std::vector<ServerCommand>& listed, std::int64_t id) {
    bool found = false;
    for (const ServerCommand& command : listed) {
        found = found || command.id == id;
    }
    return found;
}

/// Appends the item to `kept` unless it was acked and is not listed again.
void KeepOne(std::vector<DoneCommand>& kept, const DoneCommand& item, const std::vector<DoneCommand>& acked,
             const std::vector<ServerCommand>& listed) {
    bool drop = HasDone(acked, item.id) && !IsListed(listed, item.id);
    if (drop) {
        return;
    }
    kept.push_back(item);
}

/// @param character One byte.
/// @return True for a control character (below 0x20, or 0x7f).
bool IsControl(unsigned char character) { return character < 0x20 || character == 0x7f; }

/// @param text UTF-8 text.
/// @return True when any byte is a control character.
bool HasControl(const std::string& text) {
    bool found = false;
    for (char character : text) {
        found = found || IsControl(static_cast<unsigned char>(character));
    }
    return found;
}

/// Cuts UTF-8 text to a number of characters (a byte that is not a continuation byte starts one).
/// @param text The text.
/// @param chars The most characters to keep.
/// @return The cut text.
std::string CutChars(const std::string& text, std::size_t chars) {
    std::size_t seen = 0;
    std::size_t end = 0;
    while (end < text.size() && seen <= chars) {
        bool starts = (static_cast<unsigned char>(text[end]) & 0xC0) != 0x80;
        seen += starts ? 1 : 0;
        end += seen <= chars ? 1 : 0;
    }
    return text.substr(0, end);
}

/// @param done Handled commands.
/// @return The items as JSON text, comma separated, in the form of an ack: `{"command_id":..,"result":".."}`.
std::string AckItems(const std::vector<DoneCommand>& done) {
    std::string items;
    for (const DoneCommand& ack : done) {
        items += (items.empty() ? "" : ",") + std::string("{\"command_id\":") + std::to_string(ack.id) + ",\"result\":\"" + CommandResultWord(ack.result) + "\"}";
    }
    return items;
}

}  // namespace

const char* CommandResultWord(CommandResult result) {
    switch (result) {
        case CommandResult::Done: return "done";
        case CommandResult::Failed: return "failed";
        case CommandResult::Unsupported: return "unsupported";
    }
    return "failed";
}

std::optional<int> NormaliseReportInterval(std::int64_t seconds) {
    bool valid = seconds >= 60 && seconds <= 3600;
    return valid ? std::optional<int>(RoundToMinutes(seconds)) : std::nullopt;
}

std::optional<int> NormalisePingInterval(std::int64_t seconds) {
    bool valid = seconds >= 1 && seconds <= 86400;
    return valid ? std::optional<int>(std::max(60, RoundToMinutes(seconds))) : std::nullopt;
}

Reading<ServerSettings> ParseServerSettings(const std::string& text, bool versionRequired) {
    using Result = Reading<ServerSettings>;
    Reading<JsonValue> root = ParseJson(text);
    bool object = root.ok() && root.value.type == JsonType::Object;
    ServerSettings settings;
    if (!object || !ReadSettingsValues(root.value, versionRequired, settings)) {
        return Result::Failure(Status::Error, 13);
    }
    settings.checkedUnix = CheckedTime(root.value);
    return Result::Success(settings);
}

std::string FormatServerSettings(const ServerSettings& settings) {
    std::string out = "{";
    out += settings.version.has_value() ? "\"settings_version\":" + std::to_string(*settings.version) + "," : "";
    out += "\"report_interval_seconds\":" + std::to_string(settings.reportIntervalSeconds);
    out += ",\"ping_interval_seconds\":" + std::to_string(settings.pingIntervalSeconds);
    out += ",\"idle_threshold_seconds\":" + std::to_string(settings.idleThresholdSeconds);
    out += ",\"unused_threshold_seconds\":" + std::to_string(settings.unusedThresholdSeconds);
    out += settings.checkedUnix.has_value() ? ",\"last_settings_utc\":" + JsonQuote(FormatUtcTime(*settings.checkedUnix)) : "";
    return out + "}\n";
}

Reading<PingAnswer> ParsePingAnswer(const std::string& text) {
    using Result = Reading<PingAnswer>;
    Reading<JsonValue> root = ParseJson(text);
    bool object = root.ok() && root.value.type == JsonType::Object;
    if (!object) {
        return Result::Failure(Status::Error, 13);
    }
    std::optional<std::int64_t> version = WholeIn(root.value.Find("settings_version"), 0, kMaxWhole);
    const JsonValue* registered = root.value.Find("registered");
    const JsonValue* commands = root.value.Find("commands");
    bool flagged = registered != nullptr && registered->type == JsonType::Bool;
    bool listed = commands != nullptr && commands->type == JsonType::Array && commands->items.size() <= kMaxCommandsPerPing;
    if (!version || !flagged || !listed) {
        return Result::Failure(Status::Error, 13);
    }
    PingAnswer answer;
    answer.settingsVersion = *version;
    answer.registered = registered->boolean;
    for (const JsonValue& item : commands->items) {
        AddCommand(answer, item);
    }
    std::stable_sort(answer.commands.begin(), answer.commands.end(), IdLess);
    return Result::Success(answer);
}

std::string BuildPingBody(const std::string& deviceId, int unsentReports, const std::vector<DoneCommand>& acks) {
    std::string out = "{\"device_id\":" + JsonQuote(deviceId) + ",\"unsent_reports\":" + std::to_string(std::max(0, unsentReports));
    std::string items = AckItems(acks);
    out += items.empty() ? "" : ",\"acks\":[" + items + "]";
    return out + "}";
}

std::string BuildDeviceBody(const std::optional<std::string>& deviceName, const std::string& deviceType) {
    std::string name = deviceName.has_value() ? JsonQuote(*deviceName) : "null";
    return "{\"device_name\":" + name + ",\"os\":\"windows\",\"device_type\":" + JsonQuote(deviceType) + "}";
}

std::optional<std::string> DeviceNameFromFile(const std::string& fileText) {
    std::vector<std::string> lines = SplitLines(fileText);
    std::string first = lines.empty() ? std::string() : TrimBlanks(lines.front());
    if (first.empty() || HasControl(first)) {
        return std::nullopt;
    }
    return TrimBlanks(CutChars(first, kMaxDeviceNameChars));
}

std::vector<DoneCommand> ParseCommandsDone(const std::string& text) {
    std::vector<DoneCommand> done;
    Reading<JsonValue> root = ParseJson(text);
    const JsonValue* list = root.ok() ? root.value.Find("done") : nullptr;
    bool array = list != nullptr && list->type == JsonType::Array;
    for (std::size_t index = 0; array && index < list->items.size(); ++index) {
        AddDone(done, list->items[index]);
    }
    std::size_t extra = done.size() > kMaxDoneCommands ? done.size() - kMaxDoneCommands : 0;
    done.erase(done.begin(), done.begin() + static_cast<std::ptrdiff_t>(extra));
    return done;
}

std::string FormatCommandsDone(const std::vector<DoneCommand>& done) {
    std::string items;
    for (const DoneCommand& command : done) {
        items += (items.empty() ? "" : ",") + std::string("{\"id\":") + std::to_string(command.id) + ",\"result\":\"" + CommandResultWord(command.result) + "\"}";
    }
    return "{\"done\":[" + items + "]}\n";
}

bool HasDone(const std::vector<DoneCommand>& done, std::int64_t id) {
    bool found = false;
    for (const DoneCommand& command : done) {
        found = found || command.id == id;
    }
    return found;
}

std::vector<DoneCommand> WithoutAcked(const std::vector<DoneCommand>& done, const std::vector<DoneCommand>& acked,
                                      const std::vector<ServerCommand>& listed) {
    std::vector<DoneCommand> kept;
    for (const DoneCommand& item : done) {
        KeepOne(kept, item, acked, listed);
    }
    return kept;
}

}  // namespace ews
