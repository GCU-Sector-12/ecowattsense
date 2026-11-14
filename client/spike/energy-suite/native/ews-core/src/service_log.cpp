// service_log.cpp: the service log file (see ews/service_log.h).
// Used by ews-service. Only fixed words and whole numbers can be written, so no secret can reach the log.
#include "ews/service_log.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>

#include "ews/file_io.h"
#include "ews/handles.h"

namespace ews {

namespace {

constexpr std::size_t kMaxNumbersPerLine = 3;

// One word for each LogEvent, in the order of the enum.
const char* const kEventWords[] = {
    "service_start",    "service_stop",       "level_change",        "send_ok",          "send_unauthorised",
    "send_rejected",    "send_server_error",  "send_network",        "send_disabled",    "config_bad_values",
    "queue_dropped",    "queue_skipped",      "queue_broken_file",   "queue_write_failed", "history_write_failed",
    "history_cleaned",  "counted_error",      "start_failed",        "lock_held",        "model_invalid",
    "report_skipped",   "pipe_dropped",       "scan_write_failed",   "status_write_failed", "gap_detected",
    "battery_jump",     "battery_replaced",   "battery_split",
};

static_assert(sizeof(kEventWords) / sizeof(kEventWords[0]) == static_cast<std::size_t>(LogEvent::BatterySplit) + 1,
              "every LogEvent needs a word");

/// Renames the log to the previous log when it has reached the size limit. A failure (for example another
/// process renamed it first) is ignored: the line is then appended to whatever file is current.
void RollOverIfFull(const std::filesystem::path& folder) {
    WIN32_FILE_ATTRIBUTE_DATA info{};
    std::filesystem::path current = folder / L"service.log";
    bool known = GetFileAttributesExW(current.c_str(), GetFileExInfoStandard, &info) != 0;
    unsigned long long size = (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    if (known && size >= kMaxLogBytes) {
        ReplaceByRename(current, folder / L"service.previous.log");
    }
}

}  // namespace

const char* LogEventWord(LogEvent event) {
    std::size_t index = static_cast<std::size_t>(event);
    return index < sizeof(kEventWords) / sizeof(kEventWords[0]) ? kEventWords[index] : "unknown";
}

std::string FormatLogLine(UtcTime time, LogEvent event, std::initializer_list<long long> numbers) {
    std::string line = FormatUtcTimestamp(time) + " " + LogEventWord(event);
    std::size_t shown = std::min(numbers.size(), kMaxNumbersPerLine);
    const long long* number = numbers.begin();
    for (std::size_t index = 0; index < shown; ++index, ++number) {
        line += " " + std::to_string(*number);
    }
    return line + "\n";
}

Reading<bool> AppendLog(const std::wstring& dataDir, UtcTime time, LogEvent event, std::initializer_list<long long> numbers) {
    using Result = Reading<bool>;
    Result made = EnsureSubfolder(dataDir, L"logs");
    if (!made.ok()) {
        return made;
    }
    std::filesystem::path folder = std::filesystem::path(dataDir) / L"logs";
    RollOverIfFull(folder);
    std::filesystem::path path = folder / L"service.log";
    UniqueHandle file(CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) {
        return Result::FromError(GetLastError());
    }
    std::string line = FormatLogLine(time, event, numbers);
    DWORD written = 0;
    if (!WriteFile(file.get(), line.data(), static_cast<DWORD>(line.size()), &written, nullptr)) {
        return Result::FromError(GetLastError());
    }
    return written == line.size() ? Result::Success(true) : Result::Failure(Status::Error, ERROR_WRITE_FAULT);
}

}  // namespace ews
