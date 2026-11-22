// test_gpu_link.cpp: tests of the adapter roles, the NVML to adapter match and the watch targets (SPEC 1.5).
#include <string>
#include <vector>

#include "ews/gpu_link.h"
#include "test_common.h"

using ews::GpuAdapter;
using ews::GpuRole;

namespace {

/// Builds an adapter.
GpuAdapter Adapter(std::uint32_t vendor, std::uint32_t device, std::uint64_t luid, bool software = false) {
    GpuAdapter adapter;
    adapter.vendorId = vendor;
    adapter.deviceId = device;
    adapter.luid = luid;
    adapter.software = software;
    return adapter;
}

/// @return True when adapter `position` has exactly this role.
bool HasRole(const std::vector<GpuAdapter>& adapters, std::size_t position, GpuRole role) {
    return position < adapters.size() && adapters[position].role.has_value() && *adapters[position].role == role;
}

/// @return True when adapter `position` has no role.
bool NoRole(const std::vector<GpuAdapter>& adapters, std::size_t position) {
    return position < adapters.size() && !adapters[position].role.has_value();
}

/// Builds the answer of the factory.
ews::GpuPreferenceChoice Choice(std::uint64_t powerSaving, std::uint64_t highPerformance) {
    ews::GpuPreferenceChoice choice;
    choice.powerSavingLuid = powerSaving;
    choice.highPerformanceLuid = highPerformance;
    return choice;
}

/// Builds an NVML device with the given ids.
ews::NvmlDeviceFacts Nvml(std::uint32_t vendor, std::uint32_t device) {
    ews::NvmlDeviceFacts facts;
    facts.vendorId = vendor;
    facts.deviceId = device;
    return facts;
}

}  // namespace

EWS_TEST(RolesOneHardwareAdapterIsOnly) {
    std::vector<GpuAdapter> adapters{Adapter(0x1002, 0x1636, 10)};
    ews::AssignGpuRoles(adapters, Choice(10, 10));
    CHECK(HasRole(adapters, 0, GpuRole::Only));
}

EWS_TEST(RolesSoftwareAdapterHasNoRoleAndDoesNotCount) {
    std::vector<GpuAdapter> adapters{Adapter(0x1414, 0x8C, 5, true), Adapter(0x1002, 0x1636, 10)};
    ews::AssignGpuRoles(adapters, Choice(10, 10));
    CHECK(NoRole(adapters, 0));
    CHECK(HasRole(adapters, 1, GpuRole::Only));
}

EWS_TEST(RolesTwoAdaptersGetPowerSavingAndHighPerformance) {
    std::vector<GpuAdapter> adapters{Adapter(0x10DE, 0x2520, 20), Adapter(0x1002, 0x1636, 10)};
    ews::AssignGpuRoles(adapters, Choice(10, 20));
    CHECK(HasRole(adapters, 0, GpuRole::HighPerformance));
    CHECK(HasRole(adapters, 1, GpuRole::PowerSaving));
}

EWS_TEST(RolesAThirdHardwareAdapterIsOther) {
    std::vector<GpuAdapter> adapters{Adapter(0x10DE, 1, 20), Adapter(0x1002, 2, 10), Adapter(0x8086, 3, 30)};
    ews::AssignGpuRoles(adapters, Choice(10, 20));
    CHECK(HasRole(adapters, 2, GpuRole::Other));
}

EWS_TEST(RolesWhenBothPreferencesGiveTheSameAdapterEveryHardwareAdapterIsOther) {
    std::vector<GpuAdapter> adapters{Adapter(0x10DE, 1, 20), Adapter(0x1002, 2, 10)};
    ews::AssignGpuRoles(adapters, Choice(10, 10));
    CHECK(HasRole(adapters, 0, GpuRole::Other));
    CHECK(HasRole(adapters, 1, GpuRole::Other));
}

EWS_TEST(RolesWithoutAFactoryAnswerAreLeftOut) {
    std::vector<GpuAdapter> adapters{Adapter(0x10DE, 1, 20), Adapter(0x1002, 2, 10)};
    ews::AssignGpuRoles(adapters, std::nullopt);
    CHECK(NoRole(adapters, 0));
    CHECK(NoRole(adapters, 1));
    std::vector<GpuAdapter> single{Adapter(0x1002, 2, 10)};
    ews::AssignGpuRoles(single, std::nullopt);
    CHECK(NoRole(single, 0));
}

EWS_TEST(RolesAnAnswerThatNamesNoListedAdapterGivesOther) {
    std::vector<GpuAdapter> adapters{Adapter(0x10DE, 1, 20), Adapter(0x1002, 2, 10)};
    ews::AssignGpuRoles(adapters, Choice(99, 20));
    CHECK(HasRole(adapters, 0, GpuRole::Other));
    CHECK(HasRole(adapters, 1, GpuRole::Other));
}

EWS_TEST(RolesOldRolesAreReplaced) {
    std::vector<GpuAdapter> adapters{Adapter(0x1002, 2, 10)};
    adapters[0].role = GpuRole::Other;
    ews::AssignGpuRoles(adapters, std::nullopt);
    CHECK(NoRole(adapters, 0));
}

EWS_TEST(RoleWords) {
    CHECK(std::string(ews::GpuRoleWord(GpuRole::Only)) == "only");
    CHECK(std::string(ews::GpuRoleWord(GpuRole::PowerSaving)) == "power_saving");
    CHECK(std::string(ews::GpuRoleWord(GpuRole::HighPerformance)) == "high_performance");
    CHECK(std::string(ews::GpuRoleWord(GpuRole::Other)) == "other");
}

EWS_TEST(NvmlPciIdSplitsVendorAndDevice) {
    std::uint32_t vendor = 0;
    std::uint32_t device = 0;
    CHECK(ews::SplitNvmlPciId(0x252010DEu, vendor, device));
    CHECK(vendor == 0x10DE);
    CHECK(device == 0x2520);
}

EWS_TEST(NvmlPciIdWithAnotherVendorIsNotTrusted) {
    std::uint32_t vendor = 7;
    std::uint32_t device = 7;
    CHECK(!ews::SplitNvmlPciId(0x10DE2520u, vendor, device));  // halves swapped
    CHECK(!ews::SplitNvmlPciId(0, vendor, device));
    CHECK(vendor == 7 && device == 7);
}

EWS_TEST(NvmlLinkMatchesVendorAndDevice) {
    std::vector<GpuAdapter> adapters{Adapter(0x1002, 0x1636, 10), Adapter(0x10DE, 0x2520, 20)};
    std::vector<ews::NvmlDeviceFacts> devices{Nvml(0x10DE, 0x2520)};
    auto links = ews::LinkNvmlToAdapters(devices, adapters);
    CHECK(links.size() == 1);
    CHECK(links.size() == 1 && links[0].value_or(99) == 1);
}

EWS_TEST(NvmlLinkTakesEachAdapterOnce) {
    std::vector<GpuAdapter> adapters{Adapter(0x10DE, 0x2520, 20), Adapter(0x10DE, 0x2520, 21)};
    std::vector<ews::NvmlDeviceFacts> devices{Nvml(0x10DE, 0x2520), Nvml(0x10DE, 0x2520), Nvml(0x10DE, 0x2520)};
    auto links = ews::LinkNvmlToAdapters(devices, adapters);
    CHECK(links.size() == 3);
    CHECK(links.size() == 3 && links[0].value_or(99) == 0);
    CHECK(links.size() == 3 && links[1].value_or(99) == 1);
    CHECK(links.size() == 3 && !links[2].has_value());
}

EWS_TEST(NvmlLinkLeavesOutADeviceWithUnknownIdsOrNoMatch) {
    std::vector<GpuAdapter> adapters{Adapter(0x10DE, 0x2520, 20)};
    std::vector<ews::NvmlDeviceFacts> devices{Nvml(0, 0), Nvml(0x10DE, 0x1111)};
    auto links = ews::LinkNvmlToAdapters(devices, adapters);
    CHECK(links.size() == 2);
    CHECK(links.size() == 2 && !links[0].has_value());
    CHECK(links.size() == 2 && !links[1].has_value());
}

EWS_TEST(WatchTargetsAreTheHardwareAdaptersInOrder) {
    std::vector<GpuAdapter> adapters{Adapter(0x1414, 0x8C, 5, true), Adapter(0x1002, 0x1636, 10), Adapter(0x10DE, 0x2520, 20)};
    adapters[1].role = GpuRole::PowerSaving;
    auto targets = ews::HardwareAdapterTargets(adapters);
    CHECK(targets.size() == 2);
    CHECK(targets.size() == 2 && targets[0].adapter == 1 && targets[0].luid == 10 && targets[0].vendorId == 0x1002);
    CHECK(targets.size() == 2 && targets[0].role.has_value());
    CHECK(targets.size() == 2 && targets[1].adapter == 2 && targets[1].luid == 20 && !targets[1].role.has_value());
}
