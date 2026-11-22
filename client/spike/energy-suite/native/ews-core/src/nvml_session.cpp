// nvml_session.cpp: the real NVML session (see SystemNvmlSession in ews/gpu.h): name, PCI ids, power and total energy of
// NVIDIA devices. Used by ews-scan (once) and the watch mode (every period).
// NVML is optional: nvml.dll is loaded only from System32, and a missing library or function is "unavailable", never an
// error. NVML functions are declared here, no SDK file is added. Functions declared in nvml.h (NVIDIA NVML API Reference):
//   nvmlInit_v2, nvmlShutdown, nvmlDeviceGetCount_v2, nvmlDeviceGetHandleByIndex_v2, nvmlDeviceGetName,
//   nvmlDeviceGetPciInfo_v3, nvmlDeviceGetPowerUsage (mW), nvmlDeviceGetTotalEnergyConsumption (mJ since the driver loaded).
// The name, PCI and energy functions are optional (an older NVML lacks them): without them that part is unknown.
// The NVML path cannot be run on a computer without an NVIDIA card.

#include <windows.h>

#include <cstring>
#include <string>
#include <vector>

#include "ews/gpu.h"
#include "ews/gpu_link.h"

namespace ews {

namespace {

constexpr unsigned int kMaxNvmlDevices = 64;     // a sane upper bound for NVML devices
constexpr std::size_t kNameBufferBytes = 96;     // NVML_DEVICE_NAME_V2_BUFFER_SIZE; the older size is 64

// NVML signatures (nvml.h). nvmlReturn_t is an int; 0 is NVML_SUCCESS. A device handle is an opaque pointer.
using NvmlDevice = void*;
using NvmlInitFn = int(__cdecl*)();
using NvmlGetCountFn = int(__cdecl*)(unsigned int*);
using NvmlGetHandleFn = int(__cdecl*)(unsigned int, NvmlDevice*);
using NvmlGetPowerFn = int(__cdecl*)(NvmlDevice, unsigned int*);
using NvmlShutdownFn = int(__cdecl*)();
using NvmlGetNameFn = int(__cdecl*)(NvmlDevice, char*, unsigned int);
using NvmlGetEnergyFn = int(__cdecl*)(NvmlDevice, unsigned long long*);

/// nvmlPciInfo_t as the NVML reference documents it: busIdLegacy[16], domain, bus, device, pciDeviceId (16-bit device id
/// and 16-bit vendor id), pciSubSystemId, busId[32]. The struct starts zeroed, and an older NVML writes a shorter one.
struct NvmlPciInfo {
    char busIdLegacy[16];
    unsigned int domain;
    unsigned int bus;
    unsigned int device;
    unsigned int pciDeviceId;
    unsigned int pciSubSystemId;
    char busId[32];
};
using NvmlGetPciFn = int(__cdecl*)(NvmlDevice, NvmlPciInfo*);

/// Owns a loaded library and frees it in the destructor.
class LoadedLibrary {
public:
    LoadedLibrary() noexcept = default;
    LoadedLibrary(const LoadedLibrary&) = delete;
    LoadedLibrary& operator=(const LoadedLibrary&) = delete;
    ~LoadedLibrary() {
        if (module_ != nullptr) {
            FreeLibrary(module_);
        }
    }

    /// Loads a library from System32 only: a library of that name in the current folder or on PATH must never be loaded.
    /// @param name The file name.
    /// @return True when the library is loaded.
    bool Load(const wchar_t* name) {
        module_ = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        return module_ != nullptr;
    }
    /// @return The module handle; this object keeps ownership.
    HMODULE get() const noexcept { return module_; }

private:
    HMODULE module_ = nullptr;
};

/// The NVML functions this reader uses. All are null until Resolve runs.
struct NvmlApi {
    NvmlInitFn init = nullptr;
    NvmlGetCountFn getCount = nullptr;
    NvmlGetHandleFn getHandle = nullptr;
    NvmlGetPowerFn getPower = nullptr;
    NvmlShutdownFn shutdown = nullptr;
    NvmlGetNameFn getName = nullptr;      // optional
    NvmlGetPciFn getPci = nullptr;        // optional
    NvmlGetEnergyFn getEnergy = nullptr;  // optional

    /// @return True when every required function was found.
    bool complete() const { return init && getCount && getHandle && getPower && shutdown; }
};

/// Looks up one exported function.
/// @tparam Fn The function pointer type.
/// @param module The loaded library.
/// @param name The export name.
/// @return The function, or null when the library does not export it.
template <typename Fn>
Fn Resolve(HMODULE module, const char* name) {
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(module, name)));
}

/// Looks up the functions in nvml.dll.
NvmlApi ResolveNvml(HMODULE module) {
    NvmlApi api;
    api.init = Resolve<NvmlInitFn>(module, "nvmlInit_v2");
    api.getCount = Resolve<NvmlGetCountFn>(module, "nvmlDeviceGetCount_v2");
    api.getHandle = Resolve<NvmlGetHandleFn>(module, "nvmlDeviceGetHandleByIndex_v2");
    api.getPower = Resolve<NvmlGetPowerFn>(module, "nvmlDeviceGetPowerUsage");
    api.shutdown = Resolve<NvmlShutdownFn>(module, "nvmlShutdown");
    api.getName = Resolve<NvmlGetNameFn>(module, "nvmlDeviceGetName");
    api.getPci = Resolve<NvmlGetPciFn>(module, "nvmlDeviceGetPciInfo_v3");
    api.getEnergy = Resolve<NvmlGetEnergyFn>(module, "nvmlDeviceGetTotalEnergyConsumption");
    return api;
}

/// @return The name of the device as UTF-8 text, or an empty string when the function is missing or fails.
std::string ReadName(const NvmlApi& api, NvmlDevice device) {
    char name[kNameBufferBytes] = {};
    if (api.getName == nullptr || api.getName(device, name, static_cast<unsigned int>(sizeof(name))) != 0) {
        return std::string();
    }
    name[sizeof(name) - 1] = '\0';
    return std::string(name);
}

/// Reads the PCI vendor and device id of the device into the facts. They stay 0 when the function is missing, fails, or the
/// combined id does not look like an NVIDIA id (see SplitNvmlPciId).
void ReadPciIds(const NvmlApi& api, NvmlDevice device, NvmlDeviceFacts& facts) {
    if (api.getPci == nullptr) {
        return;
    }
    NvmlPciInfo info;
    std::memset(&info, 0, sizeof(info));
    if (api.getPci(device, &info) != 0) {
        return;
    }
    SplitNvmlPciId(info.pciDeviceId, facts.vendorId, facts.deviceId);
}

/// Reads power and energy of one device.
NvmlSample ReadSample(const NvmlApi& api, NvmlDevice device) {
    NvmlSample sample;
    unsigned int milliwatts = 0;
    if (device != nullptr && api.getPower(device, &milliwatts) == 0) {
        sample.milliwatts = milliwatts;
    }
    unsigned long long millijoules = 0;
    if (device != nullptr && api.getEnergy != nullptr && api.getEnergy(device, &millijoules) == 0) {
        sample.millijoules = millijoules;
    }
    return sample;
}

}  // namespace

/// The loaded library, its functions and the device handles. Handles stay valid until nvmlShutdown.
struct SystemNvmlSession::State {
    LoadedLibrary library;
    NvmlApi api;
    bool started = false;
    std::vector<NvmlDevice> devices;
};

namespace {

/// Builds the facts of one device from its static data and one sample.
NvmlDeviceFacts BuildFacts(const NvmlApi& api, NvmlDevice device) {
    NvmlDeviceFacts facts;
    facts.name = ReadName(api, device);
    ReadPciIds(api, device, facts);
    NvmlSample sample = ReadSample(api, device);
    facts.powerAnswered = sample.milliwatts.has_value();
    facts.energyAnswered = sample.millijoules.has_value();
    facts.milliwatts = sample.milliwatts;
    return facts;
}

/// Gets the handle of device `index`. @return The handle, or null when NVML refuses.
NvmlDevice HandleAt(const NvmlApi& api, unsigned int index) {
    NvmlDevice device = nullptr;
    return api.getHandle(index, &device) == 0 ? device : nullptr;
}

/// Loads NVML and starts it.
/// @return Ok, Unavailable when the library or a needed function is missing, Error when NVML fails to start.
Reading<bool> StartNvml(SystemNvmlSession::State& state) {
    if (!state.library.Load(L"nvml.dll")) {
        return Reading<bool>::Failure(Status::Unavailable, GetLastError());
    }
    state.api = ResolveNvml(state.library.get());
    if (!state.api.complete()) {
        return Reading<bool>::Failure(Status::Unavailable, ERROR_PROC_NOT_FOUND);
    }
    int started = state.api.init();
    if (started != 0) {
        return Reading<bool>::Failure(Status::Error, static_cast<unsigned long>(started));
    }
    state.started = true;
    return Reading<bool>::Success(true);
}

/// Lists the devices (handles) after NVML started.
/// @return Ok, or Error when the device count cannot be read.
Reading<bool> ListDevices(SystemNvmlSession::State& state) {
    unsigned int count = 0;
    int counted = state.api.getCount(&count);
    if (counted != 0) {
        return Reading<bool>::Failure(Status::Error, static_cast<unsigned long>(counted));
    }
    unsigned int limit = count < kMaxNvmlDevices ? count : kMaxNvmlDevices;
    state.devices.clear();
    for (unsigned int i = 0; i < limit; ++i) {
        state.devices.push_back(HandleAt(state.api, i));
    }
    return Reading<bool>::Success(true);
}

/// Starts NVML and lists the devices, once.
/// @return Ok when NVML is running (now or from an earlier call), else the failure of the step that failed.
Reading<bool> EnsureStarted(SystemNvmlSession::State& state) {
    if (state.started) {
        return Reading<bool>::Success(true);
    }
    auto started = StartNvml(state);
    if (!started.ok()) {
        return started;
    }
    return ListDevices(state);
}

}  // namespace

SystemNvmlSession::SystemNvmlSession() : state_(std::make_unique<State>()) {}

SystemNvmlSession::~SystemNvmlSession() {
    if (state_->started) {
        state_->api.shutdown();
    }
}

Reading<std::vector<NvmlDeviceFacts>> SystemNvmlSession::Open() {
    using Result = Reading<std::vector<NvmlDeviceFacts>>;
    State& state = *state_;
    auto ready = EnsureStarted(state);
    if (!ready.ok()) {
        return Result::Failure(ready.status, ready.error);
    }
    std::vector<NvmlDeviceFacts> facts;
    for (NvmlDevice device : state.devices) {
        facts.push_back(BuildFacts(state.api, device));
    }
    return Result::Success(std::move(facts));
}

Reading<std::vector<NvmlSample>> SystemNvmlSession::Read() {
    using Result = Reading<std::vector<NvmlSample>>;
    State& state = *state_;
    if (!state.started) {
        return Result::Failure(Status::Unavailable, 0);
    }
    std::vector<NvmlSample> samples;
    for (NvmlDevice device : state.devices) {
        samples.push_back(ReadSample(state.api, device));
    }
    return Result::Success(std::move(samples));
}

}  // namespace ews
