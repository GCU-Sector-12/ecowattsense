// scan_wait.cpp: the real waiter of the scan (see ews/scan.h): 3 seconds with 1 second of processor load in the middle.
// Used by ews-scan. The load makes the energy counters of a processor that is otherwise idle grow visibly.
#include <chrono>
#include <thread>

#include "ews/scan.h"

namespace ews {

namespace {

constexpr auto kPause = std::chrono::seconds(1);
constexpr auto kLoad = std::chrono::seconds(1);

/// Keeps one processor busy until the load time has passed.
/// The counter is volatile so the optimiser cannot remove the loop.
void SpinFor(std::chrono::steady_clock::duration duration) {
    const auto end = std::chrono::steady_clock::now() + duration;
    volatile unsigned long long counter = 0;
    while (std::chrono::steady_clock::now() < end) {
        counter = counter + 1;
    }
}

}  // namespace

void SystemScanWaiter::WaitWithLoad() {
    std::this_thread::sleep_for(kPause);
    SpinFor(kLoad);
    std::this_thread::sleep_for(kPause);
}

}  // namespace ews
