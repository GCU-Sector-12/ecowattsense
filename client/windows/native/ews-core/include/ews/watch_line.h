// watch_line.h: the text of the watch mode (SPEC 1.6): the header line and one line per period, as UTF-8 JSON.
// Used by watch_session.cpp and the tests. The functions take plain values and return a string.
// No Windows header is needed here. Every line ends with a line feed.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ews/watch_engine.h"

namespace ews {

/// One EMI channel in the header.
struct WatchEmiEntry {
    std::string name;
    std::string kindWord;  ///< The fixed word of the kind (cpu_package, cpu_core, ...).
    std::uint32_t device = 0;
    std::uint32_t index = 0;
};

/// One NVML device in the header.
struct WatchNvmlEntry {
    std::string name;
    std::optional<std::size_t> adapter;  ///< Index in the adapter list of the gpu source; left out when no adapter matches.
    bool power = false;
    bool energy = false;
};

/// One adapter whose use is read, in the header.
struct WatchGpuUseEntry {
    std::size_t adapter = 0;                ///< Index in the adapter list of the gpu source.
    std::optional<std::string> roleWord;    ///< Left out when unknown.
    std::uint32_t vendorId = 0;
};

/// Everything the header line holds.
struct WatchHeader {
    std::uint32_t periodMs = 0;
    std::vector<WatchEmiEntry> emi;
    std::vector<WatchNvmlEntry> nvml;
    std::vector<WatchGpuUseEntry> gpuUse;
    std::string machineJson;  ///< The machine object of capabilities.json, as JSON text.
    std::string batteryJson;  ///< The battery source object.
    std::string displayJson;  ///< The display source object.
};

/// Builds the header line: {"watch":1,"period_ms":..,"emi":[..],"nvml":[..],"gpu_use":[..],"machine":{..},"battery":{..},"display":{..}}.
/// Text is quoted as JSON (invalid UTF-8 becomes '?'). The three objects are copied as they are.
/// @param header The header values.
/// @return One line of compact JSON, ending with a line feed.
std::string BuildWatchHeaderLine(const WatchHeader& header);

/// Builds one period line: {"t_ms":..,"emi_uj":[..],"nvml_mw":[..],"nvml_mj":[..],"gpu_pct":[..]}.
/// A missing value is null; gpu_pct has one decimal and a dot.
/// @param output The values of the line.
/// @return One line of compact JSON, ending with a line feed.
std::string BuildWatchSampleLine(const WatchOutput& output);

}  // namespace ews
