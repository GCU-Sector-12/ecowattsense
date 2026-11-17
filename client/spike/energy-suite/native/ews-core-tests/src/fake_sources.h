// fake_sources.h: fake sources and a ready-made "HP laptop" scenario for the scan tests.
// Used by test_scan.cpp and test_scan_output.cpp. A fake returns what the test stored; none of them waits or touches Windows.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ews/scan.h"

namespace ews_test {

/// Fake EMI source: Open gives `channels`; the first Read gives `first`, every later Read gives `second`.
class FakeEmi final : public ews::IEmiSource {
public:
    ews::Reading<std::vector<ews::EmiChannel>> open;
    ews::Reading<std::vector<ews::EmiSample>> first;
    ews::Reading<std::vector<ews::EmiSample>> second;
    int reads = 0;

    ews::Reading<std::vector<ews::EmiChannel>> Open() override { return open; }
    ews::Reading<std::vector<ews::EmiSample>> Read() override { return reads++ == 0 ? first : second; }
};

/// Fake battery source.
class FakeBattery final : public ews::IBatterySource {
public:
    ews::Reading<std::vector<ews::BatteryState>> result;
    ews::Reading<std::vector<ews::BatteryState>> Read() override { return result; }
};

/// Fake machine source.
class FakeMachine final : public ews::IMachineInfoSource {
public:
    ews::Reading<ews::MachineInfo> result;
    ews::Reading<ews::MachineInfo> Read() override { return result; }
};

/// Fake power meter: the first Read gives `first`, every later Read gives `second`.
class FakePowerMeter final : public ews::IPowerMeterSource {
public:
    ews::Reading<std::vector<ews::PowerMeterInstance>> first;
    ews::Reading<std::vector<ews::PowerMeterInstance>> second;
    int reads = 0;
    ews::Reading<std::vector<ews::PowerMeterInstance>> Read() override { return reads++ == 0 ? first : second; }
};

/// Fake display source.
class FakeDisplay final : public ews::IDisplaySource {
public:
    ews::Reading<ews::DisplayBrightness> result;
    ews::Reading<ews::DisplayBrightness> Read() override { return result; }
};

/// Fake graphics source.
class FakeGpu final : public ews::IGpuSource {
public:
    ews::Reading<std::vector<ews::GpuAdapter>> adapters;
    ews::Reading<std::vector<ews::NvmlPower>> nvml;
    ews::Reading<std::vector<ews::GpuAdapter>> ListAdapters() override { return adapters; }
    ews::Reading<std::vector<ews::NvmlPower>> ReadNvmlPower() override { return nvml; }
};

/// Fake Intel DTT source.
class FakeDtt final : public ews::IIntelDttSource {
public:
    ews::Reading<bool> result;
    ews::Reading<bool> IsPresent() override { return result; }
};

/// Fake PawnIO source.
class FakePawnIo final : public ews::IPawnIoSource {
public:
    ews::Reading<ews::PawnIoState> result;
    ews::Reading<ews::PawnIoState> Read() override { return result; }
};

/// Fake waiter: counts the calls and does not wait.
class FakeWaiter final : public ews::IScanWaiter {
public:
    int calls = 0;
    void WaitWithLoad() override { ++calls; }
};

/// @return A fake on-battery reading: mains.
inline ews::Reading<bool> OnMains() { return ews::Reading<bool>::Success(false); }

/// One whole scenario: every source filled like the HP 250 G7 (four live EMI channels, mWh battery, no power meter,
/// display denied). A test changes the parts it cares about, then calls Sources().
class Scenario {
public:
    FakeMachine machine;
    FakeEmi emi;
    FakeBattery battery;
    FakePowerMeter powerMeter;
    FakeDisplay display;
    FakeGpu gpu;
    FakeDtt dtt;
    FakePawnIo pawnIo;
    FakeWaiter waiter;

    Scenario();

    /// @return The sources of this scenario, wired to the mains reading.
    ews::ScanSources Sources() {
        return ews::ScanSources{machine, emi, battery, powerMeter, display, gpu, dtt, pawnIo, waiter, OnMains};
    }
};

/// Builds an EMI channel for a test.
/// @param device The device index.
/// @param index The channel index.
/// @param name The channel name.
/// @return The channel, with the kind set by ClassifyEmiChannel.
ews::EmiChannel MakeChannel(std::uint32_t device, std::uint32_t index, const std::string& name);

/// Builds an EMI sample for a test.
/// @param picowattHours The energy counter.
/// @param time100ns The time in 100 ns units.
/// @return The known sample.
ews::EmiSample MakeSample(std::uint64_t picowattHours, std::uint64_t time100ns);

/// Builds one mWh battery for a test.
/// @param fullKnown Whether the full charged capacity is known.
/// @return The battery state.
ews::BatteryState MakeMwhBattery(bool fullKnown);

/// Sets one EMI channel list and the two sample lists so every channel grows, or does not grow.
/// @param scenario The scenario to change.
/// @param names The channel names, one per channel.
/// @param live One flag per channel: true when its counter grows between the reads.
void SetEmiChannels(Scenario& scenario, const std::vector<std::string>& names, const std::vector<bool>& live);

}  // namespace ews_test
