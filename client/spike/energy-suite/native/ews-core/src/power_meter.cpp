// power_meter.cpp: ACPI power meter reader through PDH (see ews/power_meter.h).
// Used by ews-scan and ews-service. PDH status codes are not Win32 errors, so they are mapped here:
// "no object", "no counter", "no instance" and "no data" mean the meter is not there (Unavailable).

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <cwchar>
#include <vector>

#include "ews/power_meter.h"
#include "ews/text.h"

#pragma comment(lib, "pdh.lib")

namespace ews {

namespace {

constexpr wchar_t kCounterPath[] = L"\\Power Meter(*)\\Power";
constexpr DWORD kMaxArrayBytes = 1024 * 1024;  // a sane upper bound for the formatted array

using Instances = Reading<std::vector<PowerMeterInstance>>;

/// Owns a PDH query and closes it in the destructor.
class PdhQuery {
public:
    PdhQuery() = default;
    PdhQuery(const PdhQuery&) = delete;
    PdhQuery& operator=(const PdhQuery&) = delete;
    ~PdhQuery() {
        if (query_ != nullptr) {
            PdhCloseQuery(query_);
        }
    }

    /// Opens the query.
    /// @return The PDH status (ERROR_SUCCESS when open).
    PDH_STATUS Open() { return PdhOpenQueryW(nullptr, 0, &query_); }
    /// @return The raw query handle.
    PDH_HQUERY get() const noexcept { return query_; }

private:
    PDH_HQUERY query_ = nullptr;
};

/// @return True when a PDH status says the counter, its object or its instances are not there.
bool IsMissingCounter(PDH_STATUS status) {
    switch (static_cast<unsigned long>(status)) {
        case PDH_CSTATUS_NO_OBJECT:
        case PDH_CSTATUS_NO_COUNTER:
        case PDH_CSTATUS_NO_INSTANCE:
        case PDH_CSTATUS_NO_MACHINE:
        case PDH_CSTATUS_BAD_COUNTERNAME:
        case PDH_INVALID_PATH:
        case PDH_NO_DATA:
        case PDH_CSTATUS_INVALID_DATA:
            return true;
        default:
            return false;
    }
}

/// Turns a failed PDH status into a failed reading.
Instances FromPdh(PDH_STATUS status) {
    unsigned long code = static_cast<unsigned long>(status);
    return Instances::Failure(IsMissingCounter(status) ? Status::Unavailable : Status::Error, code);
}

/// Reads the instance name of one array item, checking that it lies inside the buffer and ends with a terminator.
/// @param item One item of the formatted array.
/// @param begin Start of the buffer PDH filled.
/// @param end One past the end of that buffer.
/// @return The name as UTF-8, or an empty string when the pointer is outside the buffer or unterminated.
std::string ItemName(const PDH_FMT_COUNTERVALUE_ITEM_W& item, const unsigned char* begin, const unsigned char* end) {
    const unsigned char* name = reinterpret_cast<const unsigned char*>(item.szName);
    bool inside = name >= begin && name < end;
    if (!inside) {
        return std::string();
    }
    std::size_t room = static_cast<std::size_t>(end - name) / sizeof(wchar_t);
    std::size_t length = wcsnlen_s(item.szName, room);
    return length == room ? std::string() : WideToUtf8(item.szName, length);
}

/// Adds one array item to the result when its value is valid.
void AppendItem(const PDH_FMT_COUNTERVALUE_ITEM_W& item, const unsigned char* begin, const unsigned char* end,
                std::vector<PowerMeterInstance>& out) {
    bool valid = item.FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || item.FmtValue.CStatus == PDH_CSTATUS_NEW_DATA;
    if (!valid) {
        return;
    }
    out.push_back(PowerMeterInstance{ItemName(item, begin, end), item.FmtValue.doubleValue});
}

/// Reads the formatted array of a wildcard counter after one collection.
/// @param counter A counter added to a collected query.
/// @return Ok with the instances that had a valid value, or the PDH failure.
Instances ReadArray(PDH_HCOUNTER counter) {
    DWORD bytes = 0;
    DWORD count = 0;
    PDH_STATUS sizing = PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &count, nullptr);
    if (static_cast<unsigned long>(sizing) != PDH_MORE_DATA) {
        return FromPdh(sizing);
    }
    if (bytes == 0 || bytes > kMaxArrayBytes) {
        return Instances::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    std::vector<unsigned char> buffer(bytes);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
    PDH_STATUS filled = PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &count, items);
    if (static_cast<unsigned long>(filled) != ERROR_SUCCESS) {
        return FromPdh(filled);
    }
    // The count comes from the library: it must fit in the buffer before the items are read.
    if (count > bytes / sizeof(PDH_FMT_COUNTERVALUE_ITEM_W)) {
        return Instances::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    std::vector<PowerMeterInstance> out;
    const unsigned char* begin = buffer.data();
    const unsigned char* end = buffer.data() + buffer.size();
    for (DWORD i = 0; i < count; ++i) {
        AppendItem(items[i], begin, end, out);
    }
    return Instances::Success(std::move(out));
}

}  // namespace

Reading<std::vector<PowerMeterInstance>> SystemPowerMeterSource::Read() {
    PdhQuery query;
    PDH_STATUS opened = query.Open();
    if (static_cast<unsigned long>(opened) != ERROR_SUCCESS) {
        return FromPdh(opened);
    }
    PDH_HCOUNTER counter = nullptr;
    PDH_STATUS added = PdhAddEnglishCounterW(query.get(), kCounterPath, 0, &counter);
    if (static_cast<unsigned long>(added) != ERROR_SUCCESS) {
        return FromPdh(added);
    }
    PDH_STATUS collected = PdhCollectQueryData(query.get());
    if (static_cast<unsigned long>(collected) != ERROR_SUCCESS) {
        return FromPdh(collected);
    }
    return ReadArray(counter);
}

}  // namespace ews
