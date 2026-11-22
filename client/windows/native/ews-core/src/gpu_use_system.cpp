// gpu_use_system.cpp: the real GPU Engine reader through PDH (see ews/gpu_use.h).
// Used by ews-scan and the watch mode. One PDH query is kept between calls: "Utilization Percentage" is a rate, so a value
// needs two collections. The first call only primes the query and gives Unavailable.
// The counter path is added with PdhAddEnglishCounterW, so it is the same on every Windows language.

#include <windows.h>
#include <pdh.h>

#include "ews/gpu_use.h"
#include "ews/pdh_counter.h"

namespace ews {

namespace {

constexpr wchar_t kCounterPath[] = L"\\GPU Engine(*)\\Utilization Percentage";

using Instances = Reading<std::vector<PdhInstance>>;

}  // namespace

/// The query and its counter. `added` is true once the query holds the counter; `collected` once it was collected.
struct SystemGpuUseSource::State {
    PdhQuery query;
    PDH_HCOUNTER counter = nullptr;
    bool added = false;
    bool collected = false;
};

namespace {

/// Opens the query and adds the counter, once. A failure leaves `added` false, so the next call tries again.
/// @return The PDH status (ERROR_SUCCESS when the counter is in the query).
PDH_STATUS Prepare(SystemGpuUseSource::State& state) {
    if (state.added) {
        return ERROR_SUCCESS;
    }
    PDH_STATUS opened = state.query.Open();
    if (static_cast<unsigned long>(opened) != ERROR_SUCCESS) {
        return opened;
    }
    PDH_STATUS added = PdhAddEnglishCounterW(state.query.get(), kCounterPath, 0, &state.counter);
    state.added = static_cast<unsigned long>(added) == ERROR_SUCCESS;
    return added;
}

}  // namespace

SystemGpuUseSource::SystemGpuUseSource() : state_(std::make_unique<State>()) {}

SystemGpuUseSource::~SystemGpuUseSource() = default;

Reading<std::vector<PdhInstance>> SystemGpuUseSource::Read() {
    State& state = *state_;
    PDH_STATUS prepared = Prepare(state);
    if (static_cast<unsigned long>(prepared) != ERROR_SUCCESS) {
        return PdhFailure(prepared);
    }
    PDH_STATUS collected = PdhCollectQueryData(state.query.get());
    if (static_cast<unsigned long>(collected) != ERROR_SUCCESS) {
        return PdhFailure(collected);
    }
    bool primed = state.collected;
    state.collected = true;
    if (!primed) {
        return Instances::Failure(Status::Unavailable, 0);
    }
    return ReadCounterArray(state.counter);
}

}  // namespace ews
