// display_layout_system.cpp: the real display layout source (see ews/display_layout.h).
// Used by ews-scan, the service scan and the watch mode. It calls GetDisplayConfigBufferSizes and QueryDisplayConfig with
// QDC_ONLY_ACTIVE_PATHS (Microsoft Learn pages "QueryDisplayConfig function", "DISPLAYCONFIG_PATH_TARGET_INFO structure",
// "DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY enumeration"). The display configuration can change between the two calls, so the
// pair is repeated while the answer is ERROR_INSUFFICIENT_BUFFER. A path whose target is not available is not counted.
// No monitor name, device path or adapter id is read.

#include <windows.h>

#include <vector>

#include "ews/display_layout.h"

#pragma comment(lib, "user32.lib")

namespace ews {

namespace {

constexpr UINT32 kFlags = QDC_ONLY_ACTIVE_PATHS;
constexpr int kMaxAttempts = 4;
constexpr UINT32 kMaxPaths = 256;   // a sane upper bound for one computer
constexpr UINT32 kMaxModes = 1024;

/// The paths of one successful query.
struct ActivePaths {
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
};

/// Asks Windows for the active paths once.
/// @param active Receives the paths when the answer is ERROR_SUCCESS.
/// @return The Windows result code (ERROR_SUCCESS, ERROR_INSUFFICIENT_BUFFER or another failure).
LONG QueryOnce(ActivePaths& active) {
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    LONG sized = GetDisplayConfigBufferSizes(kFlags, &pathCount, &modeCount);
    if (sized != ERROR_SUCCESS) {
        return sized;
    }
    if (pathCount > kMaxPaths || modeCount > kMaxModes) {
        return ERROR_INVALID_DATA;
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    LONG queried = QueryDisplayConfig(kFlags, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
    paths.resize(queried == ERROR_SUCCESS ? pathCount : 0);
    active.paths = std::move(paths);
    return queried;
}

/// Asks for the active paths, again when the configuration changed between the two calls.
/// @return The Windows result code of the last attempt.
LONG QueryWithRetry(ActivePaths& active) {
    LONG result = ERROR_SUCCESS;
    int attempt = 0;
    do {
        result = QueryOnce(active);
        ++attempt;
    } while (result == ERROR_INSUFFICIENT_BUFFER && attempt < kMaxAttempts);
    return result;
}

/// Adds the facts of one path to the list when its target is available.
void AppendPath(const DISPLAYCONFIG_PATH_INFO& path, std::vector<DisplayPathFacts>& facts) {
    if (!path.targetInfo.targetAvailable) {
        return;
    }
    DisplayPathFacts item;
    item.internalPanel = IsInternalOutputTechnology(static_cast<std::uint32_t>(path.targetInfo.outputTechnology));
    item.refreshNumerator = path.targetInfo.refreshRate.Numerator;
    item.refreshDenominator = path.targetInfo.refreshRate.Denominator;
    facts.push_back(item);
}

}  // namespace

Reading<DisplayLayout> SystemDisplayLayoutSource::Read() {
    ActivePaths active;
    LONG result = QueryWithRetry(active);
    if (result != ERROR_SUCCESS) {
        return Reading<DisplayLayout>::FromError(static_cast<unsigned long>(result));
    }
    std::vector<DisplayPathFacts> facts;
    for (const DISPLAYCONFIG_PATH_INFO& path : active.paths) {
        AppendPath(path, facts);
    }
    return Reading<DisplayLayout>::Success(SummariseDisplayPaths(facts));
}

}  // namespace ews
