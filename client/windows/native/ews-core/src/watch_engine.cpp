// watch_engine.cpp: the pure state of the watch mode (see ews/watch_engine.h).
// Used by watch_session.cpp and the tests. Values come from counters and devices and are untrusted:
// a GPU use that is not a finite number becomes null, and every list is read with a bounds check.

#include "ews/watch_engine.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ews {

namespace {

constexpr double kMaxPercent = 100.0;

/// @return The value at `position`, or nothing when the list is shorter.
template <typename T>
std::optional<T> ValueAt(const std::vector<std::optional<T>>& values, std::size_t position) {
    return position < values.size() ? values[position] : std::optional<T>();
}

/// @return The use limited to 0..100, or nothing for a missing or not finite value.
std::optional<double> LimitedPercent(const std::optional<double>& value) {
    if (!value.has_value() || !std::isfinite(*value)) {
        return std::nullopt;
    }
    return (std::min)(kMaxPercent, (std::max)(0.0, *value));
}

}  // namespace

std::optional<std::uint64_t> CounterSinceStart::Next(const std::optional<std::uint64_t>& value) {
    if (!value.has_value()) {
        return std::nullopt;
    }
    bool wentBack = last_.has_value() && *value < *last_;
    if (!start_.has_value() || wentBack) {
        start_ = value;
        last_ = value;
        return wentBack ? std::nullopt : std::optional<std::uint64_t>(0);
    }
    last_ = value;
    return *value - *start_;
}

WatchEngine::WatchEngine(std::size_t emiCount, std::vector<bool> nvmlEnergy, std::size_t gpuCount)
    : emi_(emiCount), nvml_(nvmlEnergy.size()), nvmlEnergy_(std::move(nvmlEnergy)), gpuCount_(gpuCount) {}

WatchOutput WatchEngine::Step(const WatchInput& input) {
    WatchOutput output;
    output.tMs = input.tMs;
    for (std::size_t i = 0; i < emi_.size(); ++i) {
        output.emiUj.push_back(emi_[i].Next(ValueAt(input.emiUj, i)));
    }
    for (std::size_t i = 0; i < nvml_.size(); ++i) {
        output.nvmlMw.push_back(ValueAt(input.nvmlMw, i));
        std::optional<std::uint64_t> energy = nvmlEnergy_[i] ? ValueAt(input.nvmlMj, i) : std::nullopt;
        output.nvmlMj.push_back(nvml_[i].Next(energy));
    }
    for (std::size_t i = 0; i < gpuCount_; ++i) {
        output.gpuPct.push_back(LimitedPercent(ValueAt(input.gpuPct, i)));
    }
    return output;
}

}  // namespace ews
