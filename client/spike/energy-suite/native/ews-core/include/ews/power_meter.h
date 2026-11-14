// power_meter.h: the ACPI power meter, read as the performance counter "\Power Meter(*)\Power" through PDH.
// Used by ews-scan (source "acpi_power_meter") and ews-service (when the source is live).
#pragma once

#include <string>
#include <vector>

#include "ews/reading.h"

namespace ews {

/// One instance of the counter set.
struct PowerMeterInstance {
    std::string name;    ///< Instance name, UTF-8.
    double value = 0.0;  ///< The counter value as PDH gives it. The unit is the counter's own; it is not converted here.
};

/// A source of power meter readings. Tests use a fake.
class IPowerMeterSource {
public:
    virtual ~IPowerMeterSource() = default;
    /// Reads every instance of the counter.
    /// @return Ok with the instances that gave a valid value. Unavailable when the counter set or its instances
    ///         do not exist (no ACPI power meter). Error for any other PDH failure.
    virtual Reading<std::vector<PowerMeterInstance>> Read() = 0;
};

/// The real source: opens a PDH query, adds the counter with PdhAddEnglishCounterW (English path on every Windows language),
/// collects once and formats the values as doubles.
class SystemPowerMeterSource final : public IPowerMeterSource {
public:
    Reading<std::vector<PowerMeterInstance>> Read() override;
};

}  // namespace ews
