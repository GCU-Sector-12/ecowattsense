// model.h: the calibration model of the service (SPEC section 8) and its parser.
// Used by ews-service (reads model.json) and by the minute energy calculation. The file is written by a user
// account, so it is untrusted: every field is checked, and anything wrong makes the whole model invalid.
#pragma once

#include <string>
#include <string_view>

#include "ews/reading.h"

namespace ews {

/// The oldest schema number that ParseModelJson accepts (the `schema` key of model.json): no graphics numbers.
inline constexpr int kModelSchemaOldest = 2;

/// The newest schema number that ParseModelJson accepts: it adds the two graphics numbers.
inline constexpr int kModelSchemaNewest = 3;

/// A valid calibration model. Watt values are in W, per-percent values in W per processor percent.
struct CalibrationModel {
    int schema = kModelSchemaOldest;   ///< The `schema` of the file: 2 or 3.
    std::string generatedUtc;          ///< "YYYY-MM-DDTHH:MM:SSZ".
    bool measuresCpuPackage = false;   ///< "cpu_package" is named in measured_parts.
    bool measuresDram = false;         ///< "dram" is named in measured_parts.
    double baseW = 0;                  ///< base_w: whole computer, screen off, idle (level L3).
    double restW = 0;                  ///< rest_w: everything except the measured parts, screen off (level L2).
    double restWPerCpuPct = 0;         ///< rest_w_per_cpu_pct.
    double cpuWPerPct = 0;             ///< cpu_w_per_pct.
    double screenWAt50 = 0;            ///< screen_w_at_50.
    double screenWPerPct = 0;          ///< screen_w_per_pct.
    double graphicsW = 0;              ///< graphics_w (read and checked; not used in the sum).
    double gpuWPerPct = 0;             ///< gpu_w_per_pct (schema 3; 0 for schema 2): W per percent of GPU use, level L3.
    double restWPerGpuPct = 0;         ///< rest_w_per_gpu_pct (schema 3; 0 for schema 2): W per percent of GPU use, level L2.
};

/// Parses and checks model.json. Rules: schema is 2 or 3; generated_utc looks like YYYY-MM-DDTHH:MM:SSZ;
/// measured_parts is an array holding only "cpu_package" and "dram" (it may be empty); the seven numbers of
/// "model" are all present, finite, watt values 0 to 500 and per-percent values 0 to 5. Schema 3 also needs
/// gpu_w_per_pct and rest_w_per_gpu_pct (per-percent values 0 to 5); schema 2 does not read them and both count as 0.
/// Other keys are ignored.
/// @param text The file content (at most 64 KB).
/// @return Ok with the model, or Error (ERROR_INVALID_DATA) when anything is missing or outside the rules.
Reading<CalibrationModel> ParseModelJson(std::string_view text);

}  // namespace ews
