// display.h: brightness of the built-in display (IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS on \\.\LCD).
// Used by ews-scan (source "display") and ews-service (display power estimate).
// A desktop computer has no \\.\LCD device; that is Unavailable, not an error.
// On the test PC a standard (not elevated) user gets Denied (error 5) when opening \\.\LCD.
#pragma once

#include <cstdint>

#include "ews/reading.h"

namespace ews {

/// The two brightness levels the driver reports, in percent as the driver gave them (raw values, unchecked at this point).
struct DisplayBrightness {
    std::uint8_t onMains = 0;    ///< Level used on mains (AC).
    std::uint8_t onBattery = 0;  ///< Level used on battery (DC).
};

/// A source of display brightness. Tests use a fake.
class IDisplaySource {
public:
    virtual ~IDisplaySource() = default;
    /// Reads both brightness levels.
    /// @return Ok with the levels, Unavailable when \\.\LCD does not exist, or the Windows failure.
    virtual Reading<DisplayBrightness> Read() = 0;
};

/// The real source: opens \\.\LCD for reading and calls the video IOCTL.
class SystemDisplaySource final : public IDisplaySource {
public:
    Reading<DisplayBrightness> Read() override;
};

/// Picks the level that applies now and checks it.
/// @param levels The levels from the driver.
/// @param onBattery True when the computer runs on battery (see ReadOnBattery).
/// @return Ok with the brightness in percent (0 to 100), or Error (ERROR_INVALID_DATA) when the chosen value is above 100.
Reading<int> BrightnessPercent(const DisplayBrightness& levels, bool onBattery);

}  // namespace ews
