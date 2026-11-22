// gpu_use.cpp: the pure rules of the GPU use per adapter (see ews/gpu_use.h).
// Used by the scanner, the watch session and the tests. Instance names and values come from performance counters
// and are untrusted: names are parsed with bounds checks, values must be finite numbers at or above 0.
// The real PDH source is in gpu_use_system.cpp.

#include "ews/gpu_use.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

namespace ews {

namespace {

constexpr std::size_t kMaxHexDigits = 8;  // one 32-bit part of a LUID
constexpr double kMaxPercent = 100.0;
const std::string kLuidMarker = "luid_0x";
const std::string kEngineMarker = "_luid_";
const std::string kThreeDMarker = "engtype_3d";

/// One adapter while the instances are grouped: its engines and the sum of each engine over all processes.
struct AdapterGroup {
    std::uint64_t luid = 0;
    std::map<std::string, double> engines;
};

/// @return The character in lower case (ASCII letters only; other bytes stay as they are).
char LowerChar(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

/// @return The text in lower case.
std::string Lower(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), LowerChar);
    return lower;
}

/// A predicate: the group belongs to this adapter.
struct GroupOfLuid {
    std::uint64_t luid;
    bool operator()(const AdapterGroup& group) const { return group.luid == luid; }
};

/// A predicate: the entry belongs to this adapter.
struct UseOfLuid {
    std::uint64_t luid;
    bool operator()(const AdapterUse& use) const { return use.luid == luid; }
};

/// @return The value of a hex digit, or -1 when the character is not one.
int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/// Reads one to eight hex digits (lower case text) from the position and moves the position after them.
/// @return The value, or nothing when there is no digit at the position or there are more than eight.
std::optional<std::uint32_t> ReadHexPart(const std::string& lower, std::size_t& position) {
    std::uint64_t value = 0;
    std::size_t digits = 0;
    while (position < lower.size() && HexValue(lower[position]) >= 0 && digits <= kMaxHexDigits) {
        value = value * 16 + static_cast<std::uint64_t>(HexValue(lower[position]));
        ++position;
        ++digits;
    }
    bool good = digits >= 1 && digits <= kMaxHexDigits;
    return good ? std::optional<std::uint32_t>(static_cast<std::uint32_t>(value)) : std::nullopt;
}

/// @return True when the value can be a use in percent: a finite number at or above 0.
bool IsBelievable(double value) { return std::isfinite(value) && value >= 0.0; }

/// @return The part of the name from "_luid_" on (the engine without the process id), or the whole name.
std::string EngineKey(const std::string& name) {
    std::size_t cut = Lower(name).find(kEngineMarker);
    return cut == std::string::npos ? name : name.substr(cut);
}

/// @return The group of this adapter; it is added when it is not in the list yet.
AdapterGroup& GroupOf(std::vector<AdapterGroup>& groups, std::uint64_t luid) {
    auto found = std::find_if(groups.begin(), groups.end(), GroupOfLuid{luid});
    if (found != groups.end()) {
        return *found;
    }
    groups.push_back(AdapterGroup{luid, {}});
    return groups.back();
}

/// Adds one instance to the groups: its adapter is always noted; a 3D instance with a believable value adds to its engine.
void AddInstance(std::vector<AdapterGroup>& groups, const PdhInstance& instance) {
    auto luid = ParseInstanceLuid(instance.name);
    if (!luid.has_value()) {
        return;
    }
    AdapterGroup& group = GroupOf(groups, *luid);
    bool counts = Lower(instance.name).find(kThreeDMarker) != std::string::npos && IsBelievable(instance.value);
    if (!counts) {
        return;
    }
    group.engines[EngineKey(instance.name)] += instance.value;
}

/// @return The highest engine of the group, limited to 100, or nothing when the group has no engine.
std::optional<double> BusiestEngine(const AdapterGroup& group) {
    if (group.engines.empty()) {
        return std::nullopt;
    }
    double busiest = 0.0;
    for (const auto& engine : group.engines) {
        busiest = (std::max)(busiest, engine.second);
    }
    return (std::min)(kMaxPercent, busiest);
}

}  // namespace

std::optional<std::uint64_t> ParseInstanceLuid(const std::string& name) {
    std::string lower = Lower(name);
    std::size_t at = lower.find(kLuidMarker);
    if (at == std::string::npos) {
        return std::nullopt;
    }
    std::size_t position = at + kLuidMarker.size();
    auto high = ReadHexPart(lower, position);
    if (!high.has_value() || lower.compare(position, 3, "_0x") != 0) {
        return std::nullopt;
    }
    position += 3;
    auto low = ReadHexPart(lower, position);
    bool ends = position == lower.size() || lower[position] == '_';
    if (!low.has_value() || !ends) {
        return std::nullopt;
    }
    return (static_cast<std::uint64_t>(*high) << 32) | *low;
}

GpuUseSummary SummariseGpuUse(const std::vector<PdhInstance>& instances) {
    std::vector<AdapterGroup> groups;
    for (const PdhInstance& instance : instances) {
        AddInstance(groups, instance);
    }
    GpuUseSummary summary;
    for (const AdapterGroup& group : groups) {
        summary.adapters.push_back(AdapterUse{group.luid, BusiestEngine(group)});
    }
    return summary;
}

std::optional<double> GpuUseOfAdapter(const GpuUseSummary& summary, std::uint64_t luid) {
    auto found = std::find_if(summary.adapters.begin(), summary.adapters.end(), UseOfLuid{luid});
    return found == summary.adapters.end() ? std::nullopt : found->percent;
}

}  // namespace ews
