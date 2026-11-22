// status_keep.cpp: reads the kept fields of an old status.json (see ews/status_keep.h and SPEC 5.7).
// Used by ServiceCore::Start. Never throws; a bad file or a bad field is the same as a missing one.
#include "ews/status_keep.h"

#include <algorithm>
#include <filesystem>

#include "ews/file_io.h"
#include "ews/json_reader.h"
#include "ews/json_text.h"

namespace ews {

namespace {

/// @param root The parsed status document.
/// @param key The member name.
/// @return The text of the member, or nothing when the member is missing or not a string.
std::optional<std::string> StringMember(const JsonValue& root, const std::string& key) {
    const JsonValue* found = root.Find(key);
    bool isText = found != nullptr && found->type == JsonType::String;
    return isText ? std::optional<std::string>(found->text) : std::nullopt;
}

/// @param root The parsed status document.
/// @param key The name of a time member.
/// @param nowUnix The current time.
/// @return The Unix time when the member is a valid time not later than now plus the slack, else nothing.
std::optional<std::int64_t> TimeMember(const JsonValue& root, const std::string& key, std::int64_t nowUnix) {
    std::optional<std::string> text = StringMember(root, key);
    std::int64_t unix = 0;
    bool valid = text.has_value() && ParseUtcTime(*text, unix) && unix <= nowUnix + kKeptTimeSlackSeconds;
    return valid ? std::optional<std::int64_t>(unix) : std::nullopt;
}

}  // namespace

std::optional<LastSend> SendResultFromWord(const std::string& word) {
    static const LastSend kAll[] = {LastSend::Ok,          LastSend::Disabled, LastSend::Unauthorised, LastSend::Rejected,
                                    LastSend::ServerError, LastSend::Network,  LastSend::Never};
    const LastSend* end = kAll + sizeof(kAll) / sizeof(kAll[0]);
    const LastSend* found = std::find_if(kAll, end, [&word](LastSend candidate) { return word == SendResultWord(candidate); });
    return found != end ? std::optional<LastSend>(*found) : std::nullopt;
}

KeptStatus ReadKeptStatus(const std::wstring& dataDir, std::int64_t nowUnix) {
    KeptStatus kept;
    std::filesystem::path file = std::filesystem::path(dataDir) / L"status" / L"status.json";
    Reading<std::string> text = ReadWholeFile(file, kMaxStatusFileBytes);
    if (!text.ok()) {
        return kept;
    }
    Reading<JsonValue> root = ParseJson(text.value);
    if (!root.ok()) {
        return kept;
    }
    kept.lastReportUnix = TimeMember(root.value, "last_report_utc", nowUnix);
    kept.lastSendUnix = TimeMember(root.value, "last_send_utc", nowUnix);
    std::optional<std::string> word = StringMember(root.value, "last_send_result");
    kept.lastSendResult = word.has_value() ? SendResultFromWord(*word) : std::nullopt;
    return kept;
}

}  // namespace ews
