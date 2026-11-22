// history_file.cpp: the daily history CSV file (see ews/history_file.h).
// Used by ews-service. Each row opens the file, appends and closes it, so no handle is held over a change of day.
#include "ews/history_file.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "ews/file_io.h"
#include "ews/handles.h"

namespace ews {

namespace {

constexpr std::size_t kNameLength = 14;  // "yyyy-MM-dd.csv"

/// @return True for a character that makes a line break.
bool IsLineBreak(char c) { return c == '\n' || c == '\r'; }

/// @return True when the text has no line break.
bool IsSingleLine(const std::string& text) { return std::none_of(text.begin(), text.end(), IsLineBreak); }

/// @return The digit value of one wide character, or -1 when it is not a digit.
int DigitValue(wchar_t c) { return (c >= L'0' && c <= L'9') ? c - L'0' : -1; }

/// Reads `count` digits of a name from `start`. @return The number, or -1 when one is not a digit.
int ReadDigits(const std::wstring& name, std::size_t start, std::size_t count) {
    int number = 0;
    for (std::size_t index = start; index < start + count; ++index) {
        int digit = DigitValue(name[index]);
        number = number < 0 || digit < 0 ? -1 : number * 10 + digit;
    }
    return number;
}

/// Reads the date of a history file name.
/// @return The day, or nothing when the name is not exactly `yyyy-MM-dd.csv` with a real date.
std::optional<std::chrono::sys_days> ParseHistoryDate(const std::wstring& name) {
    bool shapeOk = name.size() == kNameLength && name[4] == L'-' && name[7] == L'-' && name.compare(10, 4, L".csv") == 0;
    if (!shapeOk) {
        return std::nullopt;
    }
    int year = ReadDigits(name, 0, 4);
    int month = ReadDigits(name, 5, 2);
    int day = ReadDigits(name, 8, 2);
    bool numbersOk = year >= 0 && month >= 0 && day >= 0;
    std::chrono::year_month_day date{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(std::max(month, 0))},
                                     std::chrono::day{static_cast<unsigned>(std::max(day, 0))}};
    if (!numbersOk || !date.ok()) {
        return std::nullopt;
    }
    return std::chrono::sys_days{date};
}

/// Writes all bytes to an open file in one call. @return Ok with true, or the failure.
Reading<bool> WriteAll(const UniqueHandle& file, const std::string& bytes) {
    DWORD written = 0;
    if (!WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)) {
        return Reading<bool>::FromError(GetLastError());
    }
    if (written != bytes.size()) {
        return Reading<bool>::Failure(Status::Error, ERROR_WRITE_FAULT);
    }
    return Reading<bool>::Success(true);
}

/// Deletes one directory entry when it is a history file older than the cutoff. @return 1 when deleted, otherwise 0.
int DeleteIfOld(const std::filesystem::directory_entry& entry, std::chrono::sys_days cutoff) {
    std::error_code ignored;
    std::optional<std::chrono::sys_days> date = ParseHistoryDate(entry.path().filename().wstring());
    bool oldFile = date.has_value() && *date < cutoff;
    bool plainFile = oldFile && std::filesystem::symlink_status(entry.path(), ignored).type() == std::filesystem::file_type::regular;
    return (plainFile && std::filesystem::remove(entry.path(), ignored)) ? 1 : 0;
}

}  // namespace

std::wstring HistoryFileName(UtcTime time) {
    std::string date = FormatUtcDate(time);
    return std::wstring(date.begin(), date.end()) + L".csv";
}

bool IsHistoryFileName(const std::wstring& name) { return ParseHistoryDate(name).has_value(); }

Reading<bool> AppendHistoryRow(const std::wstring& dataDir, UtcTime time, const std::string& header, const std::string& row) {
    using Result = Reading<bool>;
    if (row.empty() || !IsSingleLine(row) || !IsSingleLine(header)) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    Result folder = EnsureSubfolder(dataDir, L"history");
    if (!folder.ok()) {
        return folder;
    }
    std::filesystem::path path = std::filesystem::path(dataDir) / L"history" / HistoryFileName(time);
    UniqueHandle file(CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) {
        return Result::FromError(GetLastError());
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size)) {
        return Result::FromError(GetLastError());
    }
    // A new or empty file starts with the header; the header and the row go out in one write.
    std::string bytes = (size.QuadPart == 0 ? header + "\n" : std::string()) + row + "\n";
    return WriteAll(file, bytes);
}

Reading<int> DeleteOldHistoryFiles(const std::wstring& dataDir, UtcTime now) {
    using Result = Reading<int>;
    std::filesystem::path folder = std::filesystem::path(dataDir) / L"history";
    std::error_code error;
    std::filesystem::directory_iterator iterator(folder, error);
    if (error.value() == ERROR_PATH_NOT_FOUND || error.value() == ERROR_FILE_NOT_FOUND) {
        return Result::Success(0);
    }
    if (error) {
        return Result::FromError(static_cast<unsigned long>(error.value()));
    }
    std::chrono::sys_days today = std::chrono::floor<std::chrono::days>(now);
    std::chrono::sys_days cutoff = today - std::chrono::days(kHistoryKeepDays);
    int deleted = 0;
    for (; iterator != std::filesystem::directory_iterator(); iterator.increment(error)) {
        deleted += DeleteIfOld(*iterator, cutoff);
    }
    return Result::Success(deleted);
}

}  // namespace ews
