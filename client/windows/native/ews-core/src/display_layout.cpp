// display_layout.cpp: the pure rules of the display layout (see ews/display_layout.h).
// Used by the scanner and the tests. The real source (QueryDisplayConfig) is in display_layout_system.cpp.

#include "ews/display_layout.h"

#include <algorithm>

namespace ews {

namespace {

constexpr std::uint32_t kInternal = 0x80000000u;  // DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL
constexpr std::uint32_t kLvds = 6;                // DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS
constexpr std::uint32_t kDisplayPortEmbedded = 11;
constexpr std::uint32_t kUdiEmbedded = 13;
constexpr std::size_t kMaxPaths = 256;            // a sane upper bound for one computer
constexpr std::uint64_t kMaxHertz = 1000;

/// @return True when the path is the built-in panel.
bool IsInternalPath(const DisplayPathFacts& path) { return path.internalPanel; }

/// @return True when the path is not the built-in panel.
bool IsExternalPath(const DisplayPathFacts& path) { return !path.internalPanel; }

/// @return The rate of the path in whole hertz, or nothing.
std::optional<std::uint32_t> PathHertz(const DisplayPathFacts& path) {
    return RefreshHertz(path.refreshNumerator, path.refreshDenominator);
}

}  // namespace

bool IsInternalOutputTechnology(std::uint32_t technology) {
    return technology == kInternal || technology == kLvds || technology == kDisplayPortEmbedded || technology == kUdiEmbedded;
}

std::optional<std::uint32_t> RefreshHertz(std::uint32_t numerator, std::uint32_t denominator) {
    if (denominator == 0) {
        return std::nullopt;
    }
    std::uint64_t rounded = (static_cast<std::uint64_t>(numerator) + denominator / 2) / denominator;
    bool believable = rounded >= 1 && rounded <= kMaxHertz;
    return believable ? std::optional<std::uint32_t>(static_cast<std::uint32_t>(rounded)) : std::nullopt;
}

DisplayLayout SummariseDisplayPaths(std::span<const DisplayPathFacts> paths) {
    DisplayLayout layout;
    if (paths.size() > kMaxPaths) {
        return layout;
    }
    layout.displays = static_cast<std::uint32_t>(paths.size());
    layout.externalDisplays = static_cast<std::uint32_t>(std::count_if(paths.begin(), paths.end(), IsExternalPath));
    if (paths.empty()) {
        return layout;
    }
    auto builtIn = std::find_if(paths.begin(), paths.end(), IsInternalPath);
    const DisplayPathFacts& chosen = builtIn != paths.end() ? *builtIn : paths.front();
    layout.refreshHz = PathHertz(chosen);
    return layout;
}

}  // namespace ews
