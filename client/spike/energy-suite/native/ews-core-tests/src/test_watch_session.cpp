// test_watch_session.cpp: tests of the watch session (SPEC 1.6) with fake sources: header, lines, failed sources.
#include <string>
#include <vector>

#include "ews/watch_session.h"
#include "fake_sources.h"
#include "test_common.h"

using ews::Status;
using ews_test::Scenario;

namespace {

/// The sources of a watch over a scenario and a fake NVML session.
struct Rig {
    Scenario scenario;
    ews_test::FakeNvmlSession nvml;

    /// @return The watch sources.
    ews::WatchSources Sources() {
        return ews::WatchSources{scenario.machine, scenario.emi, scenario.battery, scenario.display, scenario.displayLayout,
                                 scenario.gpu,     nvml,         scenario.gpuUse,   ews_test::OnMains};
    }
};

/// @return True when `text` holds `part`.
bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

/// Builds an adapter with a LUID.
ews::GpuAdapter Adapter(std::uint32_t vendor, std::uint32_t device, std::uint64_t luid) {
    ews::GpuAdapter adapter;
    adapter.description = "Test";
    adapter.vendorId = vendor;
    adapter.deviceId = device;
    adapter.luid = luid;
    return adapter;
}

/// Builds the NVML facts of a device that answers power and energy.
ews::NvmlDeviceFacts Nvidia() {
    ews::NvmlDeviceFacts facts;
    facts.name = "NVIDIA Test";
    facts.vendorId = 0x10DE;
    facts.deviceId = 0x2520;
    facts.powerAnswered = true;
    facts.energyAnswered = true;
    facts.milliwatts = 15000;
    return facts;
}

/// Builds a sample of the NVML device.
ews::NvmlSample NvmlAt(std::optional<std::uint32_t> mw, std::optional<std::uint64_t> mj) {
    ews::NvmlSample sample;
    sample.milliwatts = mw;
    sample.millijoules = mj;
    return sample;
}

}  // namespace

EWS_TEST(WatchSessionHeaderOfThePlainHpScenario) {
    Rig rig;
    ews::WatchSession session(rig.Sources(), 2000);
    auto header = session.Start();
    CHECK(Has(header, "{\"watch\":1,\"period_ms\":2000,\"emi\":[{\"name\":\"RAPL_Package0_PKG\",\"kind\":\"cpu_package\""));
    CHECK(Has(header, "\"name\":\"RAPL_Package0_PP1\",\"kind\":\"igpu\""));
    CHECK(Has(header, "\"nvml\":[]"));
    CHECK(Has(header, "\"gpu_use\":[{\"adapter\":0,\"vendor_id\":32902}]"));  // the fake adapter has no role
    CHECK(Has(header, "\"machine\":{ \"maker\": \"HP\""));
    CHECK(Has(header, "\"battery\":{ \"id\": \"battery\""));
    CHECK(Has(header, "\"display\":{ \"id\": \"display\""));
    CHECK(header.find('\n') == header.size() - 1);
    CHECK(rig.scenario.emi.reads == 0);  // the header does not read the counters
}

EWS_TEST(WatchSessionLinesFollowTheCounters) {
    Rig rig;
    ews::WatchSession session(rig.Sources(), 2000);
    session.Start();
    auto first = session.Sample(0);
    CHECK(first == "{\"t_ms\":0,\"emi_uj\":[0,0,0,0],\"nvml_mw\":[],\"nvml_mj\":[],\"gpu_pct\":[null]}\n");
    auto second = session.Sample(3000);
    CHECK(second == "{\"t_ms\":3000,\"emi_uj\":[12300001,12300001,12300001,12300001],\"nvml_mw\":[],\"nvml_mj\":[],\"gpu_pct\":[null]}\n");
}

EWS_TEST(WatchSessionAFailedEmiReadIsNull) {
    Rig rig;
    rig.scenario.emi.second = ews::Reading<std::vector<ews::EmiSample>>::Failure(Status::Error, 5);
    ews::WatchSession session(rig.Sources(), 2000);
    session.Start();
    session.Sample(0);
    CHECK(Has(session.Sample(2000), "\"emi_uj\":[null,null,null,null]"));
}

EWS_TEST(WatchSessionNvmlAndGpuUsePerAdapter) {
    Rig rig;
    rig.scenario.gpu.adapters = ews::Reading<std::vector<ews::GpuAdapter>>::Success({Adapter(0x8086, 0x9A49, 0xA1B2), Adapter(0x10DE, 0x2520, 0xC3D4)});
    rig.nvml.open = ews::Reading<std::vector<ews::NvmlDeviceFacts>>::Success({Nvidia()});
    rig.nvml.reads = {ews::Reading<std::vector<ews::NvmlSample>>::Success({NvmlAt(15300, 98765)}),
                      ews::Reading<std::vector<ews::NvmlSample>>::Success({NvmlAt(std::nullopt, 99000)})};
    rig.scenario.gpuUse.second = ews::Reading<std::vector<ews::PdhInstance>>::Success(
        {{"pid_1_luid_0x00000000_0x0000A1B2_phys_0_eng_0_engtype_3D", 12.5},
         {"pid_9_luid_0x00000000_0x0000C3D4_phys_0_eng_0_engtype_3D", 90.0}});
    ews::WatchSession session(rig.Sources(), 2000);
    auto header = session.Start();
    CHECK(Has(header, "\"nvml\":[{\"name\":\"NVIDIA Test\",\"adapter\":1,\"power\":true,\"energy\":true}]"));
    CHECK(Has(header, "\"gpu_use\":[{\"adapter\":0,\"vendor_id\":32902},{\"adapter\":1,\"vendor_id\":4318}]"));
    auto first = session.Sample(0);
    CHECK(Has(first, "\"nvml_mw\":[15300],\"nvml_mj\":[0],\"gpu_pct\":[12.5,90.0]"));
    auto second = session.Sample(2000);
    CHECK(Has(second, "\"nvml_mw\":[null],\"nvml_mj\":[235]"));
}

EWS_TEST(WatchSessionTheGpuEngineQueryIsPrimedByStart) {
    Rig rig;
    rig.scenario.gpuUse.first = ews::Reading<std::vector<ews::PdhInstance>>::Success({{"pid_1_luid_0x0_0x1_phys_0_eng_0_engtype_3D", 55.0}});
    ews::WatchSession session(rig.Sources(), 2000);
    session.Start();
    CHECK(rig.scenario.gpuUse.reads == 1);  // Start read once; its answer is not used
    CHECK(Has(session.Sample(0), "\"gpu_pct\":[null]"));
}

EWS_TEST(WatchSessionFailedNvmlOpenGivesAnEmptyList) {
    Rig rig;
    rig.nvml.open = ews::Reading<std::vector<ews::NvmlDeviceFacts>>::Failure(Status::Error, 9);
    ews::WatchSession session(rig.Sources(), 2000);
    CHECK(Has(session.Start(), "\"nvml\":[]"));
    CHECK(Has(session.Sample(0), "\"nvml_mw\":[],\"nvml_mj\":[]"));
    CHECK(rig.nvml.next == 0);  // never read when it did not open
}

EWS_TEST(WatchSessionNvmlWithoutEnergyKeepsEnergyNull) {
    Rig rig;
    auto facts = Nvidia();
    facts.energyAnswered = false;
    rig.nvml.open = ews::Reading<std::vector<ews::NvmlDeviceFacts>>::Success({facts});
    rig.nvml.reads = {ews::Reading<std::vector<ews::NvmlSample>>::Success({NvmlAt(15300, 98765)})};
    ews::WatchSession session(rig.Sources(), 2000);
    CHECK(Has(session.Start(), "\"power\":true,\"energy\":false"));
    CHECK(Has(session.Sample(0), "\"nvml_mw\":[15300],\"nvml_mj\":[null]"));
}

EWS_TEST(WatchSessionFailedNvmlReadGivesNulls) {
    Rig rig;
    rig.nvml.open = ews::Reading<std::vector<ews::NvmlDeviceFacts>>::Success({Nvidia()});
    rig.nvml.reads = {ews::Reading<std::vector<ews::NvmlSample>>::Failure(Status::Error, 3)};
    ews::WatchSession session(rig.Sources(), 2000);
    session.Start();
    CHECK(Has(session.Sample(0), "\"nvml_mw\":[null],\"nvml_mj\":[null]"));
}

EWS_TEST(WatchSessionSurvivesEverySourceFailing) {
    Rig rig;
    rig.scenario.machine.result = ews::Reading<ews::MachineInfo>::Failure(Status::Error, 1);
    rig.scenario.emi.open = ews::Reading<std::vector<ews::EmiChannel>>::Failure(Status::Error, 2);
    rig.scenario.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Failure(Status::Error, 3);
    rig.scenario.gpu.adapters = ews::Reading<std::vector<ews::GpuAdapter>>::Failure(Status::Error, 4);
    ews::WatchSession session(rig.Sources(), 2000);
    auto header = session.Start();
    CHECK(Has(header, "\"emi\":[],\"nvml\":[],\"gpu_use\":[]"));
    CHECK(Has(header, "\"machine\":{ }"));
    CHECK(session.Sample(0) == "{\"t_ms\":0,\"emi_uj\":[],\"nvml_mw\":[],\"nvml_mj\":[],\"gpu_pct\":[]}\n");
}
