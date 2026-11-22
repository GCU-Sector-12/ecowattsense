// test_gpu_use.cpp: tests of the GPU use per adapter from "GPU Engine" instances (SPEC 1.5).
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "ews/gpu_use.h"
#include "test_common.h"

using ews::PdhInstance;

namespace {

/// Builds an instance name like Windows does.
std::string Name(int pid, const std::string& luidHighLow, int engine, const std::string& type) {
    return "pid_" + std::to_string(pid) + "_luid_" + luidHighLow + "_phys_0_eng_" + std::to_string(engine) + "_engtype_" + type;
}

const std::string kIntel = "0x00000000_0x0000A1B2";
const std::string kNvidia = "0x00000000_0x0000C3D4";

/// @return True when the value is near the wanted one.
bool Near(const std::optional<double>& value, double wanted) { return value.has_value() && std::fabs(*value - wanted) < 1e-9; }

}  // namespace

EWS_TEST(GpuUseParsesTheLuidOfAnInstanceName) {
    auto luid = ews::ParseInstanceLuid(Name(12, kIntel, 0, "3D"));
    CHECK(luid.value_or(0) == 0xA1B2ULL);
    auto big = ews::ParseInstanceLuid("pid_1_luid_0x00000001_0x0000FFFF_phys_0_eng_0_engtype_3D");
    CHECK(big.value_or(0) == ((1ULL << 32) | 0xFFFFULL));
    auto lower = ews::ParseInstanceLuid("pid_1_luid_0x00000000_0x0000a1b2_phys_0");
    CHECK(lower.value_or(0) == 0xA1B2ULL);
}

EWS_TEST(GpuUseRejectsBadInstanceNames) {
    CHECK(!ews::ParseInstanceLuid("").has_value());
    CHECK(!ews::ParseInstanceLuid("pid_1_phys_0_eng_0").has_value());
    CHECK(!ews::ParseInstanceLuid("pid_1_luid_0x_0x1_phys_0").has_value());
    CHECK(!ews::ParseInstanceLuid("pid_1_luid_0xZZ_0x1_phys_0").has_value());
    CHECK(!ews::ParseInstanceLuid("pid_1_luid_0x00000000_0x123456789_phys_0").has_value());  // nine digits
    CHECK(!ews::ParseInstanceLuid("pid_1_luid_0x00000000").has_value());
}

EWS_TEST(GpuUseSumsProcessesOfOneEngineAndTakesTheHighestEngine) {
    std::vector<PdhInstance> instances{
        {Name(1, kIntel, 0, "3D"), 10.0}, {Name(2, kIntel, 0, "3D"), 15.5},  // engine 0: 25.5
        {Name(1, kIntel, 1, "3D"), 20.0},                                    // engine 1: 20
    };
    auto summary = ews::SummariseGpuUse(instances);
    CHECK(Near(ews::GpuUseOfAdapter(summary, 0xA1B2), 25.5));
}

EWS_TEST(GpuUseIsLimitedTo100) {
    std::vector<PdhInstance> instances{{Name(1, kIntel, 0, "3D"), 80.0}, {Name(2, kIntel, 0, "3D"), 70.0}};
    CHECK(Near(ews::GpuUseOfAdapter(ews::SummariseGpuUse(instances), 0xA1B2), 100.0));
}

EWS_TEST(GpuUseCountsOnlyThe3DEnginesCaseInsensitive) {
    std::vector<PdhInstance> instances{{Name(1, kIntel, 0, "Copy"), 50.0}, {Name(1, kIntel, 1, "VideoDecode"), 40.0},
                                       {Name(1, kIntel, 2, "3d"), 12.0}};
    auto summary = ews::SummariseGpuUse(instances);
    CHECK(Near(ews::GpuUseOfAdapter(summary, 0xA1B2), 12.0));
}

EWS_TEST(GpuUseKeepsAdaptersApart) {
    std::vector<PdhInstance> instances{{Name(1, kIntel, 0, "3D"), 12.5}, {Name(1, kNvidia, 0, "3D"), 90.0}};
    auto summary = ews::SummariseGpuUse(instances);
    CHECK(summary.adapters.size() == 2);
    CHECK(Near(ews::GpuUseOfAdapter(summary, 0xA1B2), 12.5));
    CHECK(Near(ews::GpuUseOfAdapter(summary, 0xC3D4), 90.0));
}

EWS_TEST(GpuUseCountsEveryAdapterSeenInAnyInstanceName) {
    std::vector<PdhInstance> instances{{Name(1, kIntel, 0, "3D"), 1.0}, {Name(1, kNvidia, 0, "Copy"), 2.0},
                                       {Name(2, kNvidia, 1, "Copy"), 3.0}, {"garbage", 4.0}};
    auto summary = ews::SummariseGpuUse(instances);
    CHECK(summary.adapters.size() == 2);                              // the name without an id is ignored
    CHECK(!ews::GpuUseOfAdapter(summary, 0xC3D4).has_value());        // seen, but no 3D instance: unknown
}

EWS_TEST(GpuUseOfAnAdapterThatWasNotSeenIsUnknown) {
    std::vector<PdhInstance> instances{{Name(1, kIntel, 0, "3D"), 5.0}};
    CHECK(!ews::GpuUseOfAdapter(ews::SummariseGpuUse(instances), 0xC3D4).has_value());
    CHECK(!ews::GpuUseOfAdapter(ews::SummariseGpuUse({}), 0xA1B2).has_value());
}

EWS_TEST(GpuUseIgnoresValuesThatAreNotBelievable) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    double inf = std::numeric_limits<double>::infinity();
    std::vector<PdhInstance> instances{{Name(1, kIntel, 0, "3D"), nan}, {Name(2, kIntel, 0, "3D"), inf},
                                       {Name(3, kIntel, 0, "3D"), -5.0}, {Name(4, kIntel, 1, "3D"), 7.0}};
    CHECK(Near(ews::GpuUseOfAdapter(ews::SummariseGpuUse(instances), 0xA1B2), 7.0));
    std::vector<PdhInstance> onlyBad{{Name(1, kIntel, 0, "3D"), nan}};
    CHECK(!ews::GpuUseOfAdapter(ews::SummariseGpuUse(onlyBad), 0xA1B2).has_value());
}
