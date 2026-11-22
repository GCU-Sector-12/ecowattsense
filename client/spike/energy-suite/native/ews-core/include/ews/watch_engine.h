// watch_engine.h: the pure state of the watch mode (SPEC 1.6): turns the raw values of each period into the values
// of one output line. Energy counters become "since the first good read"; a counter that went back gives null once
// and a new start value; GPU use is limited to 0..100.
// Used by watch_session.cpp and the tests. No Windows header is needed here.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ews {

/// The raw values of one period. The lists must have the length given to the engine; a shorter list counts as null.
struct WatchInput {
    std::int64_t tMs = 0;                                   ///< Milliseconds on a monotonic clock since the watch started.
    std::vector<std::optional<std::uint64_t>> emiUj;        ///< Absolute energy of each EMI channel in microjoules.
    std::vector<std::optional<std::uint32_t>> nvmlMw;       ///< Power of each NVML device in milliwatts.
    std::vector<std::optional<std::uint64_t>> nvmlMj;       ///< Absolute energy counter of each NVML device in millijoules.
    std::vector<std::optional<double>> gpuPct;              ///< Use of each adapter in percent.
};

/// The values of one output line. The lists have the length given to the engine.
struct WatchOutput {
    std::int64_t tMs = 0;
    std::vector<std::optional<std::uint64_t>> emiUj;        ///< Microjoules since the first good read; null for a failed read.
    std::vector<std::optional<std::uint32_t>> nvmlMw;
    std::vector<std::optional<std::uint64_t>> nvmlMj;       ///< Millijoules since the first good read; always null without energy.
    std::vector<std::optional<double>> gpuPct;              ///< 0..100, or null.
};

/// One energy counter that is followed since its first good read.
class CounterSinceStart {
public:
    /// Takes the next absolute value.
    /// @param value The value, or nothing for a failed read.
    /// @return The value since the first good read (0 for the first good one). Nothing for a failed read, and once for a
    ///         value below the previous one; that value then becomes the new start.
    std::optional<std::uint64_t> Next(const std::optional<std::uint64_t>& value);

private:
    std::optional<std::uint64_t> start_;
    std::optional<std::uint64_t> last_;
};

/// The state of the watch mode.
class WatchEngine {
public:
    /// @param emiCount Number of EMI channels.
    /// @param nvmlEnergy One flag per NVML device: true when its energy counter answers. A false flag keeps nvml_mj null.
    /// @param gpuCount Number of GPU use values.
    WatchEngine(std::size_t emiCount, std::vector<bool> nvmlEnergy, std::size_t gpuCount);

    /// Turns the raw values of one period into one line of values.
    /// @param input The raw values.
    /// @return The values of the line; the lists have the lengths given to the constructor.
    WatchOutput Step(const WatchInput& input);

private:
    std::vector<CounterSinceStart> emi_;
    std::vector<CounterSinceStart> nvml_;
    std::vector<bool> nvmlEnergy_;
    std::size_t gpuCount_;
};

}  // namespace ews
