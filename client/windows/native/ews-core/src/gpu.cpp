// gpu.cpp: graphics adapter list (DXGI), the role of each adapter, and the NVIDIA power reads (see ews/gpu.h).
// Used by ews-scan and the watch mode. The NVML code itself is in nvml_session.cpp; the pure decisions are in gpu_link.cpp.
// DXGI calls: CreateDXGIFactory1, IDXGIFactory1::EnumAdapters1, IDXGIAdapter1::GetDesc1 and, for the roles,
// IDXGIFactory6::EnumAdapterByGpuPreference (dxgi1_6.h, Windows 10 version 1803 and later; Microsoft Learn pages
// "IDXGIFactory6::EnumAdapterByGpuPreference" and "DXGI_ADAPTER_DESC1").
// When the factory interface is not there, the roles are left out.

#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>

#include "ews/gpu.h"
#include "ews/gpu_link.h"
#include "ews/text.h"

#pragma comment(lib, "dxgi.lib")

namespace ews {

namespace {

constexpr UINT kMaxAdapters = 64;  // a sane upper bound for one computer
constexpr std::uint64_t kBytesPerMegabyte = 1024ULL * 1024ULL;

using Microsoft::WRL::ComPtr;

/// @return The LUID as (high part << 32) | low part. The high part is signed, so it is cast to unsigned first.
std::uint64_t LuidValue(const LUID& luid) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(luid.HighPart)) << 32) | static_cast<std::uint64_t>(luid.LowPart);
}

/// Converts the description of one adapter and adds it to the list.
void AppendAdapter(IDXGIAdapter1* adapter, std::vector<GpuAdapter>& adapters) {
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc))) {
        return;
    }
    GpuAdapter item;
    item.description = WideToUtf8(desc.Description, wcsnlen_s(desc.Description, ARRAYSIZE(desc.Description)));
    item.vendorId = desc.VendorId;
    item.deviceId = desc.DeviceId;
    item.dedicatedMemoryMegabytes = static_cast<std::uint64_t>(desc.DedicatedVideoMemory) / kBytesPerMegabyte;
    item.software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    item.luid = LuidValue(desc.AdapterLuid);
    adapters.push_back(std::move(item));
}

/// @return The LUID of the adapter when it is a hardware adapter, else nothing.
std::optional<std::uint64_t> HardwareLuidOf(IDXGIAdapter1* adapter) {
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
        return std::nullopt;
    }
    return LuidValue(desc.AdapterLuid);
}

/// Asks the factory for the adapter at `index` in the order of a preference.
/// @return True when the factory returned an adapter.
bool NextByPreference(IDXGIFactory6* factory, DXGI_GPU_PREFERENCE preference, UINT index, ComPtr<IDXGIAdapter1>& adapter) {
    return factory->EnumAdapterByGpuPreference(index, preference, IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())) == S_OK;
}

/// @return The LUID of the first hardware adapter the factory gives for a preference, or nothing.
std::optional<std::uint64_t> FirstHardwareLuid(IDXGIFactory6* factory, DXGI_GPU_PREFERENCE preference) {
    std::optional<std::uint64_t> found;
    ComPtr<IDXGIAdapter1> adapter;
    UINT index = 0;
    while (!found.has_value() && index < kMaxAdapters && NextByPreference(factory, preference, index, adapter)) {
        found = HardwareLuidOf(adapter.Get());
        ++index;
    }
    return found;
}

/// Asks the factory which hardware adapter it prefers for minimum power and for high performance.
/// @return Both answers, or nothing when the factory interface is missing or one answer is missing.
std::optional<GpuPreferenceChoice> ReadPreferences(IDXGIFactory1* factory) {
    ComPtr<IDXGIFactory6> factory6;
    if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
        return std::nullopt;
    }
    auto powerSaving = FirstHardwareLuid(factory6.Get(), DXGI_GPU_PREFERENCE_MINIMUM_POWER);
    auto highPerformance = FirstHardwareLuid(factory6.Get(), DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE);
    if (!powerSaving.has_value() || !highPerformance.has_value()) {
        return std::nullopt;
    }
    return GpuPreferenceChoice{*powerSaving, *highPerformance};
}

}  // namespace

Reading<std::vector<GpuAdapter>> SystemGpuSource::ListAdapters() {
    using Result = Reading<std::vector<GpuAdapter>>;
    ComPtr<IDXGIFactory1> factory;
    HRESULT created = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(created)) {
        return Result::Failure(Status::Error, static_cast<unsigned long>(created));
    }
    std::vector<GpuAdapter> adapters;
    ComPtr<IDXGIAdapter1> adapter;
    UINT index = 0;
    while (index < kMaxAdapters && factory->EnumAdapters1(index, adapter.ReleaseAndGetAddressOf()) == S_OK) {
        AppendAdapter(adapter.Get(), adapters);
        ++index;
    }
    AssignGpuRoles(adapters, ReadPreferences(factory.Get()));
    return Result::Success(std::move(adapters));
}

Reading<std::vector<NvmlDeviceFacts>> SystemGpuSource::ReadNvmlDevices() {
    SystemNvmlSession session;
    return session.Open();
}

Reading<std::vector<NvmlPower>> SystemGpuSource::ReadNvmlPower() {
    using Result = Reading<std::vector<NvmlPower>>;
    auto devices = ReadNvmlDevices();
    if (!devices.ok()) {
        return Result::Failure(devices.status, devices.error);
    }
    std::vector<NvmlPower> powers;
    for (const NvmlDeviceFacts& device : devices.value) {
        powers.push_back(device.powerAnswered ? NvmlPower{true, device.milliwatts.value_or(0)} : NvmlPower{});
    }
    return Result::Success(std::move(powers));
}

}  // namespace ews
