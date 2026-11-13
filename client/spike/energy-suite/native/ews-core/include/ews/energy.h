// energy.h: the energy of one minute (SPEC 4.2 and 4.5), as a pure function over values.
// Used by ews-service at every tick, and by the tests. The service works in energy, not in watts.
// An unknown value never becomes 0: a minute that cannot be calculated has no energy.
#pragma once

#include <optional>

#include "ews/model.h"

namespace ews {

/// The measurement level of one minute.
enum class MeasureLevel {
    Unknown,  ///< The minute could not be calculated.
    L1,       ///< The EMI "system" channel measured the whole computer.
    L2,       ///< Processor package and memory measured, the rest from the model.
    L3        ///< Everything from the model.
};

/// @param level A level.
/// @return "L1", "L2" or "L3"; an empty text for Unknown.
const char* MeasureLevelWord(MeasureLevel level);

/// What the service knows about one minute. An std::optional that is empty is an unknown value.
struct MinuteInput {
    double seconds = 0;                         ///< Real length of the minute, in seconds (must be above 0).
    std::optional<double> cpuPercent;           ///< Processor load, 0 to 100.
    std::optional<double> systemGrowthUj;       ///< Growth of the EMI "system" kind, in uJ.
    std::optional<double> packageGrowthUj;      ///< Growth of the EMI "cpu_package" kind, in uJ.
    std::optional<double> dramGrowthUj;         ///< Growth of the EMI "dram" kind, in uJ.
    bool displayOn = true;                      ///< The display is on.
    std::optional<double> brightnessPct;        ///< Brightness 0 to 100; unknown counts as 50.
    const CalibrationModel* model = nullptr;    ///< The valid model, or null when there is none.
};

/// The energy of one minute.
struct MinuteEnergy {
    MeasureLevel level = MeasureLevel::Unknown;
    std::optional<double> deviceUj;  ///< Energy of the whole computer in uJ; empty when the minute is unknown.
    double measuredUj = 0;           ///< The part read from hardware, in uJ (0 for L3 and for an unknown minute).
};

/// Calculates one minute by SPEC 4.5: L1 when the system growth is known; else L2 when the model is valid, names
/// at least one measured part, the growth of every named part is known and the processor percent is known;
/// else L3 when the model is valid and the processor percent is known; else unknown.
/// A negative or non-finite growth, a percent outside 0 to 100 and a length of 0 or less count as unknown.
/// Screen power is 0 with the display off, else screen_w_at_50 + (brightness - 50) x screen_w_per_pct, never below 0.
/// @param input The values of the minute.
/// @return The level, the device energy and the measured part.
MinuteEnergy CalculateMinuteEnergy(const MinuteInput& input);

}  // namespace ews
