// watch_line.cpp: the text of the watch mode (see ews/watch_line.h).
// Used by watch_session.cpp and the tests. The JSON is built by hand, compact, one line per call.
// Numbers use a dot and no locale. Text is quoted by JsonQuote, so invalid UTF-8 never reaches the output.

#include "ews/watch_line.h"

#include "ews/json_text.h"

namespace ews {

namespace {

constexpr int kPercentDecimals = 1;

/// @return `[a,b]` for the items, `[]` for none.
std::string List(const std::vector<std::string>& items) {
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        out += (i == 0 ? "" : ",") + items[i];
    }
    return out + "]";
}

/// @return The whole number as text, or "null" when it is unknown.
template <typename T>
std::string NumberOrNull(const std::optional<T>& value) {
    return value.has_value() ? std::to_string(static_cast<std::uint64_t>(*value)) : "null";
}

/// @return The percentage with one decimal, or "null" when it is unknown or cannot be formatted.
std::string PercentOrNull(const std::optional<double>& value) {
    if (!value.has_value()) {
        return "null";
    }
    std::string text = FormatFixed(*value, kPercentDecimals);
    return text.empty() ? "null" : text;
}

/// @return `true` or `false`.
std::string BoolText(bool value) { return value ? "true" : "false"; }

/// @return The list of whole numbers, null for unknown ones.
template <typename T>
std::string NumberList(const std::vector<std::optional<T>>& values) {
    std::vector<std::string> items;
    for (const auto& value : values) {
        items.push_back(NumberOrNull(value));
    }
    return List(items);
}

/// @return The list of percentages, null for unknown ones.
std::string PercentList(const std::vector<std::optional<double>>& values) {
    std::vector<std::string> items;
    for (const auto& value : values) {
        items.push_back(PercentOrNull(value));
    }
    return List(items);
}

/// @return One EMI entry of the header.
std::string EmiEntryText(const WatchEmiEntry& entry) {
    return "{\"name\":" + JsonQuote(entry.name) + ",\"kind\":" + JsonQuote(entry.kindWord) +
           ",\"device\":" + std::to_string(entry.device) + ",\"index\":" + std::to_string(entry.index) + "}";
}

/// @return `"adapter":N,` or nothing when the adapter is unknown.
std::string AdapterField(const std::optional<std::size_t>& adapter) {
    return adapter.has_value() ? "\"adapter\":" + std::to_string(*adapter) + "," : std::string();
}

/// @return One NVML entry of the header.
std::string NvmlEntryText(const WatchNvmlEntry& entry) {
    return "{\"name\":" + JsonQuote(entry.name) + "," + AdapterField(entry.adapter) + "\"power\":" + BoolText(entry.power) +
           ",\"energy\":" + BoolText(entry.energy) + "}";
}

/// @return `"role":"word",` or nothing when the role is unknown.
std::string RoleField(const std::optional<std::string>& roleWord) {
    return roleWord.has_value() ? "\"role\":" + JsonQuote(*roleWord) + "," : std::string();
}

/// @return One GPU use entry of the header.
std::string GpuUseEntryText(const WatchGpuUseEntry& entry) {
    return "{\"adapter\":" + std::to_string(entry.adapter) + "," + RoleField(entry.roleWord) +
           "\"vendor_id\":" + std::to_string(entry.vendorId) + "}";
}

}  // namespace

std::string BuildWatchHeaderLine(const WatchHeader& header) {
    std::vector<std::string> emi;
    for (const WatchEmiEntry& entry : header.emi) {
        emi.push_back(EmiEntryText(entry));
    }
    std::vector<std::string> nvml;
    for (const WatchNvmlEntry& entry : header.nvml) {
        nvml.push_back(NvmlEntryText(entry));
    }
    std::vector<std::string> gpuUse;
    for (const WatchGpuUseEntry& entry : header.gpuUse) {
        gpuUse.push_back(GpuUseEntryText(entry));
    }
    std::string line = "{\"watch\":1,\"period_ms\":" + std::to_string(header.periodMs);
    line += ",\"emi\":" + List(emi) + ",\"nvml\":" + List(nvml) + ",\"gpu_use\":" + List(gpuUse);
    line += ",\"machine\":" + header.machineJson + ",\"battery\":" + header.batteryJson + ",\"display\":" + header.displayJson;
    return line + "}\n";
}

std::string BuildWatchSampleLine(const WatchOutput& output) {
    std::string line = "{\"t_ms\":" + std::to_string(output.tMs);
    line += ",\"emi_uj\":" + NumberList(output.emiUj) + ",\"nvml_mw\":" + NumberList(output.nvmlMw);
    line += ",\"nvml_mj\":" + NumberList(output.nvmlMj) + ",\"gpu_pct\":" + PercentList(output.gpuPct);
    return line + "}\n";
}

}  // namespace ews
