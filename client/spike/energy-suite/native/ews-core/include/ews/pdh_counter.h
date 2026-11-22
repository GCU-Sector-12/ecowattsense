// pdh_counter.h: the small PDH helpers shared by the readers of performance counters (ACPI power meter, GPU Engine):
// an owner of a PDH query, the mapping of PDH status codes, and the read of a wildcard counter as a list of instances.
// Used by power_meter.cpp and gpu_use_system.cpp. PDH status codes are not Win32 errors, so they are mapped here.
#pragma once

#include <windows.h>
#include <pdh.h>

#include <vector>

#include "ews/pdh_instance.h"
#include "ews/reading.h"

namespace ews {

/// Owns a PDH query and closes it in the destructor.
class PdhQuery {
public:
    PdhQuery() = default;
    PdhQuery(const PdhQuery&) = delete;
    PdhQuery& operator=(const PdhQuery&) = delete;
    ~PdhQuery();

    /// Opens the query.
    /// @return The PDH status (ERROR_SUCCESS when open).
    PDH_STATUS Open();
    /// @return The raw query handle.
    PDH_HQUERY get() const noexcept { return query_; }

private:
    PDH_HQUERY query_ = nullptr;
};

/// Turns a failed PDH status into a failed reading: "no object", "no counter", "no instance" and "no data" mean the
/// counter is not there (Unavailable); anything else is an Error.
/// @param status The failed PDH status.
/// @return The failed reading, with the PDH status as the error code.
Reading<std::vector<PdhInstance>> PdhFailure(PDH_STATUS status);

/// Reads the formatted array of a wildcard counter after one collection.
/// @param counter A counter added to a collected query.
/// @return Ok with the instances that had a valid value, or the PDH failure.
Reading<std::vector<PdhInstance>> ReadCounterArray(PDH_HCOUNTER counter);

}  // namespace ews
