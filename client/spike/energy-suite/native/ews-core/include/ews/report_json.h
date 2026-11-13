// report_json.h: builds the JSON text of one report (SPEC 5.4), by hand, with exactly the fields of the table.
// Used by ews-service before it queues a report. The server refuses unknown fields, so nothing else is written.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ews/reading.h"

namespace ews {

/// The values of one report. An empty optional is an unknown value: the field is left out, never sent as 0 or null.
struct ReportFields {
    std::string deviceId;                          ///< GUID in lower case with hyphens.
    std::int64_t timestampUnix = 0;                ///< End of the interval, Unix seconds.
    std::int64_t intervalSeconds = 0;              ///< At least 1.
    std::int64_t idleSeconds = 0;                  ///< At least 0.
    int state = 0;                                 ///< 2, 1 or 0.
    std::optional<double> cpuPercent;              ///< 0 to 100.
    bool screenLocked = false;
    bool onBattery = false;
    std::string deviceType;                        ///< ^[a-z][a-z0-9_]*$, at most 40 characters.
    std::optional<std::int64_t> energyUj;
    std::optional<std::int64_t> batteryEnergyUj;
    std::optional<std::int64_t> deviceEnergyUj;
};

/// Builds the report JSON in the order of SPEC 5.4: device_id, timestamp, interval_seconds, idle_seconds, state,
/// cpu_percent (one decimal), screen_locked, on_battery, os ("windows"), device_type, energy_uj, battery_energy_uj,
/// device_energy_uj. The text is one line with no line break and no spaces.
/// @param fields The values.
/// @return Ok with the text, or Error (ERROR_INVALID_DATA) when device_type is not ^[a-z][a-z0-9_]*$ or longer
///         than 40 characters, interval_seconds is below 1, idle_seconds is negative or state is not 0, 1 or 2.
Reading<std::string> BuildReportJson(const ReportFields& fields);

}  // namespace ews
