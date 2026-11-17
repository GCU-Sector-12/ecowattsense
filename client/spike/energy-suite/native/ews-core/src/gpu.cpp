// gpu.cpp: graphics adapter list (DXGI) and NVIDIA power (NVML, loaded at run time) (see ews/gpu.h).
// Used by ews-scan. NVML is optional: nvml.dll is loaded only from System32, and a missing library or function is
// "unavailable", never an error. The NVML path cannot be run on a computer without an NVIDIA card.

#include <windows.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <vector>

#include "ews/gpu.h"
#include "ews/text.h"

#pragma comment(lib, "dxgi.lib")

namespace ews {

namespace {

constexpr UINT kMaxAdapters = 64;                      // a sane upper bound for one computer
constexpr unsigned int kMaxNvmlDevices = 64;           // the same for NVML devices
constexpr std::uint64_t kBytesPerMegabyte = 1024ULL * 1024ULL;

using Microsoft::WRL::ComPtr;

// NVML signatures (nvml.h). nvmlReturn_t is an int; 0 is NVML_SUCCESS. A device handle is an opaque pointer.
using NvmlDevice = void*;
using NvmlInitFn = int(__cdecl*)();
using NvmlGetCountFn = int(__cdecl*)(unsigned int*);
using NvmlGetHandleFn = int(__cdecl*)(unsigned int, NvmlDevice*);
using NvmlGetPowerFn = int(__cdecl*)(NvmlDevice, unsigned int*);
using NvmlShutdownFn = int(__cdecl*)();

/// Owns a loaded library and frees it in the destructor.
class LoadedLibrary {
public:
    explicit LoadedLibrary(HMODULE module) noexcept : module_(module) {}
    LoadedLibrary(const LoadedLibrary&) = delete;
    LoadedLibrary& operator=(const LoadedLibrary&) = delete;
    ~LoadedLibrary() {
        if (module_ != nullptr) {
            FreeLibrary(module_);
        }
    }

    /// @return True when the library is loaded.
    bool valid() const noexcept { return module_ != nullptr; }
    /// @return The module handle; this object keeps ownership.
    HMODULE get() const noexcept { return module_; }

private:
    HMODULE module_;
};

/// The five NVML functions this reader uses. All are null until Resolve succeeds.
struct NvmlApi {
    NvmlInitFn init = nullptr;
    NvmlGetCountFn getCount = nullptr;
    NvmlGetHandleFn getHandle = nullptr;
    NvmlGetPowerFn getPower = nullptr;
    NvmlShutdownFn shutdown = nullptr;

    /// @return True when every function was found.
    bool complete() const { return init && getCount && getHandle && getPower && shutdown; }
};

/// Looks up one exported function.
/// @tparam Fn The function pointer type.
/// @param module The loaded library.
/// @param name The export name.
/// @return The function, or null when the library does not export it.
template <typename Fn>
Fn Resolve(HMODULE module, const char* name) {
    return reinterpret_cast<Fn>(GetProcAddress(module, name));
}

/// Looks up the five functions in nvml.dll.
NvmlApi ResolveNvml(HMODULE module) {
    NvmlApi api;
    api.init = Resolve<NvmlInitFn>(module, "nvmlInit_v2");
    api.getCount = Resolve<NvmlGetCountFn>(module, "nvmlDeviceGetCount_v2");
    api.getHandle = Resolve<NvmlGetHandleFn>(module, "nvmlDeviceGetHandleByIndex_v2");
    api.getPower = Resolve<NvmlGetPowerFn>(module, "nvmlDeviceGetPowerUsage");
    api.shutdown = Resolve<NvmlShutdownFn>(module, "nvmlShutdown");
    return api;
}

/// Reads the power of one NVML device by index. Any NVML error gives an unknown entry.
NvmlPower ReadOneDevice(const NvmlApi& api, unsigned int index) {
    NvmlDevice device = nullptr;
    unsigned int milliwatts = 0;
    bool got = api.getHandle(index, &device) == 0 && api.getPower(device, &milliwatts) == 0;
    return got ? NvmlPower{true, milliwatts} : NvmlPower{};
}

/// Reads every device after NVML started.
/// @return Ok with one entry per device, or Error when the device count cannot be read.
Reading<std::vector<NvmlPower>> ReadDevices(const NvmlApi& api) {
    unsigned int count = 0;
    int counted = api.getCount(&count);
    if (counted != 0) {
        return Reading<std::vector<NvmlPower>>::Failure(Status::Error, static_cast<unsigned long>(counted));
    }
    std::vector<NvmlPower> powers;
    unsigned int limit = count < kMaxNvmlDevices ? count : kMaxNvmlDevices;
    for (unsigned int i = 0; i < limit; ++i) {
        powers.push_back(ReadOneDevice(api, i));
    }
    return Reading<std::vector<NvmlPower>>::Success(std::move(powers));
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
    item.dedicatedMemoryMegabytes = static_cast<std::uint64_t>(desc.DedicatedVideoMemory) / kBytesPerMegabyte;
    item.software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    adapters.push_back(std::move(item));
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
    return Result::Success(std::move(adapters));
}

Reading<std::vector<NvmlPower>> SystemGpuSource::ReadNvmlPower() {
    using Result = Reading<std::vector<NvmlPower>>;
    // System32 only: a library of that name in the current folder or on PATH must never be loaded.
    LoadedLibrary library(LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (!library.valid()) {
        return Result::Failure(Status::Unavailable, GetLastError());
    }
    NvmlApi api = ResolveNvml(library.get());
    if (!api.complete()) {
        return Result::Failure(Status::Unavailable, ERROR_PROC_NOT_FOUND);
    }
    int started = api.init();
    if (started != 0) {
        return Result::Failure(Status::Error, static_cast<unsigned long>(started));
    }
    auto powers = ReadDevices(api);
    api.shutdown();
    return powers;
}

}  // namespace ews
