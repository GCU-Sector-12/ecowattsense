// power_meter.cpp: ACPI power meter reader through PDH (see ews/power_meter.h).
// Used by ews-scan and ews-service. The PDH helpers (query owner, status mapping, array read) are in pdh_counter.cpp.

#include "ews/power_meter.h"

#include "ews/pdh_counter.h"

namespace ews {

namespace {

constexpr wchar_t kCounterPath[] = L"\\Power Meter(*)\\Power";

}  // namespace

Reading<std::vector<PowerMeterInstance>> SystemPowerMeterSource::Read() {
    PdhQuery query;
    PDH_STATUS opened = query.Open();
    if (static_cast<unsigned long>(opened) != ERROR_SUCCESS) {
        return PdhFailure(opened);
    }
    PDH_HCOUNTER counter = nullptr;
    PDH_STATUS added = PdhAddEnglishCounterW(query.get(), kCounterPath, 0, &counter);
    if (static_cast<unsigned long>(added) != ERROR_SUCCESS) {
        return PdhFailure(added);
    }
    PDH_STATUS collected = PdhCollectQueryData(query.get());
    if (static_cast<unsigned long>(collected) != ERROR_SUCCESS) {
        return PdhFailure(collected);
    }
    return ReadCounterArray(counter);
}

}  // namespace ews
