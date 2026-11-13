// test_calc_common.h: fixtures shared by the calculation tests (model of SPEC section 8, a fixed time, number compare).
// Used by test_energy.cpp, test_interval.cpp, test_report_json.cpp and test_history_status.cpp.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "ews/model.h"

namespace ews_test {

/// 2026-11-14T10:05:00Z as Unix seconds.
constexpr std::int64_t kTestTime = 1794650700;

/// @return True when the two numbers differ by less than 0.5 (half a microjoule).
inline bool Near(double a, double b) { return std::fabs(a - b) < 0.5; }

/// @return The model of SPEC section 8 with both EMI kinds measured.
inline ews::CalibrationModel SpecModel() {
    ews::CalibrationModel m;
    m.generatedUtc = "2026-11-12T17:08:31Z";
    m.measuresCpuPackage = true;
    m.measuresDram = true;
    m.baseW = 3.48;
    m.restW = 2.24;
    m.restWPerCpuPct = 0.009;
    m.cpuWPerPct = 0.075;
    m.screenWAt50 = 1.40;
    m.screenWPerPct = 0.0104;
    m.graphicsW = 2.44;
    return m;
}

/// @return The text of the model file of SPEC section 8.
inline std::string SpecModelText() {
    return R"({
  "schema": 2,
  "generated_utc": "2026-11-12T17:08:31Z",
  "machine": { "maker": "HP", "model": "HP 250 G7 Notebook PC", "cpu": "x", "logical_processors": 8, "windows": "10.0.26300.0" },
  "measured_parts": ["cpu_package", "dram"],
  "model": {
    "base_w": 3.48,
    "rest_w": 2.24,
    "rest_w_per_cpu_pct": 0.009,
    "cpu_w_per_pct": 0.075,
    "screen_w_at_50": 1.40,
    "screen_w_per_pct": 0.0104,
    "graphics_w": 2.44
  },
  "phase_power_w": { },
  "cpu_share_by_phase": { },
  "warnings": []
})";
}

}  // namespace ews_test
