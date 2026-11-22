// display_layout.h: how many displays are active, how many are not the built-in panel, and the refresh rate.
// Used by ews-scan (source "display", fields displays, external_displays, refresh_hz) and the watch mode header.
// The decision functions are pure and need no Windows header; the real source uses QueryDisplayConfig.
#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "ews/reading.h"

namespace ews {

/// What the scanner keeps of one active display path.
struct DisplayPathFacts {
    bool internalPanel = false;              ///< The output is the built-in panel.
    std::uint32_t refreshNumerator = 0;      ///< Refresh rate of the path, as a rational number.
    std::uint32_t refreshDenominator = 0;
};

/// The display layout. An absent value means "unknown".
struct DisplayLayout {
    std::optional<std::uint32_t> displays;          ///< Active displays.
    std::optional<std::uint32_t> externalDisplays;  ///< Active displays that are not the built-in panel.
    std::optional<std::uint32_t> refreshHz;         ///< Of the built-in panel, or of the first display when there is none.
};

/// Tells if a DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY value is a built-in connection: INTERNAL (0x80000000),
/// LVDS (6), DISPLAYPORT_EMBEDDED (11) or UDI_EMBEDDED (13).
/// @param technology The value of outputTechnology.
/// @return True for a built-in panel.
bool IsInternalOutputTechnology(std::uint32_t technology);

/// Turns a rational refresh rate into whole hertz, rounded to the nearest.
/// @param numerator The numerator.
/// @param denominator The denominator.
/// @return The hertz, or nothing when the denominator is 0, the rate is below 1 Hz or above 1000 Hz.
std::optional<std::uint32_t> RefreshHertz(std::uint32_t numerator, std::uint32_t denominator);

/// Summarises the active display paths.
/// @param paths The active paths whose target is available.
/// @return displays = number of paths, externalDisplays = paths that are not the built-in panel, refreshHz of the
///         first built-in path (else the first path). Everything is unknown when there are more than 256 paths.
///         refreshHz is unknown when there is no path or the chosen path has no usable rate.
DisplayLayout SummariseDisplayPaths(std::span<const DisplayPathFacts> paths);

/// A source of the display layout. Tests use a fake.
class IDisplayLayoutSource {
public:
    virtual ~IDisplayLayoutSource() = default;
    /// Reads the layout.
    /// @return Ok with the layout, or the failure from Windows (unknown, never "no display").
    virtual Reading<DisplayLayout> Read() = 0;
};

/// The real source: GetDisplayConfigBufferSizes and QueryDisplayConfig with QDC_ONLY_ACTIVE_PATHS.
class SystemDisplayLayoutSource final : public IDisplayLayoutSource {
public:
    Reading<DisplayLayout> Read() override;
};

}  // namespace ews
