// battery.cpp: battery reader through SetupAPI and the battery class IOCTLs (see ews/battery.h).
// Used by ews-scan and ews-service. Numbers from the driver are untrusted: the "unknown" markers
// are turned into *Known = false, never into 0.
// No device path, serial number or battery name is kept: the path lives only inside one call.

#include <windows.h>
#include <winioctl.h>
#include <batclass.h>
#include <setupapi.h>

#include <string>
#include <vector>

#include "ews/battery.h"
#include "ews/handles.h"

#pragma comment(lib, "setupapi.lib")

namespace ews {

namespace {

// GUID_DEVICE_BATTERY: the battery device interface class. Defined here to avoid the initguid.h include-order trap.
constexpr GUID kBatteryInterface = {0x72631e54, 0x78a4, 0x11d0, {0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a}};
constexpr std::uint64_t kMicrojoulesPerMilliwattHour = 3600000ULL;
constexpr std::uint32_t kUnknownU32 = 0xFFFFFFFFu;               // BATTERY_UNKNOWN_CAPACITY and BATTERY_UNKNOWN_VOLTAGE
constexpr std::int32_t kUnknownRate = static_cast<std::int32_t>(0x80000000u);  // BATTERY_UNKNOWN_RATE
constexpr std::size_t kMaxBatteries = 64;                        // a sane upper bound for one computer

/// The device paths found by the enumeration, and the first error met on the way (0 = none).
struct PathList {
    std::vector<std::wstring> paths;
    unsigned long error = 0;
};

/// Reads the device path of one enumerated interface and appends it to the list.
/// @param devices The device information set.
/// @param data The interface returned by SetupDiEnumDeviceInterfaces.
/// @param list The list to extend; list.error is set when the path cannot be read.
void AppendPath(HDEVINFO devices, SP_DEVICE_INTERFACE_DATA& data, PathList& list) {
    DWORD size = 0;
    SetupDiGetDeviceInterfaceDetailW(devices, &data, nullptr, 0, &size, nullptr);  // asks for the size; fails on purpose
    if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size > 4096) {
        list.error = ERROR_INVALID_DATA;
        return;
    }
    std::vector<unsigned char> buffer(size);
    auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    if (!SetupDiGetDeviceInterfaceDetailW(devices, &data, detail, size, nullptr, nullptr)) {
        list.error = GetLastError();
        return;
    }
    buffer[size - 1] = 0;  // the string must end inside the buffer
    buffer[size - 2] = 0;
    list.paths.emplace_back(detail->DevicePath);
}

/// Lists the device paths of every present battery interface.
/// @return The paths; error is 0 when the enumeration ended normally (also when there is no battery).
PathList ListBatteryPaths() {
    PathList list;
    UniqueDevInfo devices(SetupDiGetClassDevsW(&kBatteryInterface, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (!devices.valid()) {
        list.error = GetLastError();
        return list;
    }
    SP_DEVICE_INTERFACE_DATA data{};
    data.cbSize = sizeof(data);
    DWORD index = 0;
    while (list.paths.size() < kMaxBatteries && SetupDiEnumDeviceInterfaces(devices.get(), nullptr, &kBatteryInterface, index, &data)) {
        AppendPath(devices.get(), data, list);
        ++index;
    }
    DWORD last = GetLastError();
    bool endedNormally = last == ERROR_NO_MORE_ITEMS || list.paths.size() >= kMaxBatteries;
    list.error = list.error != 0 ? list.error : (endedNormally ? 0 : last);
    return list;
}

/// Asks the battery for its tag (IOCTL_BATTERY_QUERY_TAG).
/// @param device An open battery handle.
/// @return Ok with the tag; Unavailable when the slot is empty (tag 0) or the driver says so.
Reading<ULONG> QueryTag(HANDLE device) {
    ULONG wait = 0;
    ULONG tag = 0;
    DWORD returned = 0;
    if (!DeviceIoControl(device, IOCTL_BATTERY_QUERY_TAG, &wait, sizeof(wait), &tag, sizeof(tag), &returned, nullptr)) {
        return Reading<ULONG>::FromError(GetLastError());
    }
    if (returned != sizeof(tag) || tag == BATTERY_TAG_INVALID) {
        return Reading<ULONG>::Failure(Status::Unavailable, 0);
    }
    return Reading<ULONG>::Success(tag);
}

/// Reads the static information of the battery (BatteryInformation).
/// @param device An open battery handle.
/// @param tag The tag from QueryTag.
/// @return Ok with the information, or the failure from Windows.
Reading<BATTERY_INFORMATION> QueryInformation(HANDLE device, ULONG tag) {
    BATTERY_QUERY_INFORMATION query{};
    query.BatteryTag = tag;
    query.InformationLevel = BatteryInformation;
    BATTERY_INFORMATION info{};
    DWORD returned = 0;
    if (!DeviceIoControl(device, IOCTL_BATTERY_QUERY_INFORMATION, &query, sizeof(query), &info, sizeof(info), &returned, nullptr)) {
        return Reading<BATTERY_INFORMATION>::FromError(GetLastError());
    }
    if (returned < sizeof(info)) {
        return Reading<BATTERY_INFORMATION>::Failure(Status::Error, ERROR_INSUFFICIENT_BUFFER);
    }
    return Reading<BATTERY_INFORMATION>::Success(info);
}

/// Reads the live status of the battery (capacity, voltage, rate, power state).
/// @param device An open battery handle.
/// @param tag The tag from QueryTag.
/// @return Ok with the status, or the failure from Windows.
Reading<BATTERY_STATUS> QueryStatus(HANDLE device, ULONG tag) {
    BATTERY_WAIT_STATUS wait{};
    wait.BatteryTag = tag;
    BATTERY_STATUS status{};
    DWORD returned = 0;
    if (!DeviceIoControl(device, IOCTL_BATTERY_QUERY_STATUS, &wait, sizeof(wait), &status, sizeof(status), &returned, nullptr)) {
        return Reading<BATTERY_STATUS>::FromError(GetLastError());
    }
    if (returned < sizeof(status)) {
        return Reading<BATTERY_STATUS>::Failure(Status::Error, ERROR_INSUFFICIENT_BUFFER);
    }
    return Reading<BATTERY_STATUS>::Success(status);
}

/// Sets one capacity field and its Known flag from a driver value.
/// @param value The driver value; 0xFFFFFFFF means unknown.
/// @param known Receives true when the value is usable.
/// @param target Receives the value, or 0 when unknown.
void SetCapacity(std::uint32_t value, bool& known, std::uint32_t& target) {
    known = value != kUnknownU32;
    target = known ? value : 0;
}

/// Combines the information and status structures into a BatteryState.
/// @param info The static information.
/// @param status The live status.
/// @return The state, with unknown markers turned into *Known = false.
BatteryState BuildState(const BATTERY_INFORMATION& info, const BATTERY_STATUS& status) {
    BatteryState state;
    state.unit = (info.Capabilities & BATTERY_CAPACITY_RELATIVE) != 0 ? BatteryUnit::Relative : BatteryUnit::MilliwattHours;
    SetCapacity(status.Capacity, state.remainingKnown, state.remaining);
    SetCapacity(info.FullChargedCapacity, state.fullChargedKnown, state.fullCharged);
    SetCapacity(info.DesignedCapacity, state.designedKnown, state.designed);
    SetCapacity(status.Voltage, state.voltageKnown, state.voltage);
    state.rateKnown = status.Rate != kUnknownRate;
    state.rate = state.rateKnown ? status.Rate : 0;
    state.onLine = (status.PowerState & BATTERY_POWER_ON_LINE) != 0;
    state.charging = (status.PowerState & BATTERY_CHARGING) != 0;
    state.discharging = (status.PowerState & BATTERY_DISCHARGING) != 0;
    return state;
}

/// Reads one battery from its device path.
/// @param path The device path; it is not stored.
/// @return Ok with the state, or the first failure (open, tag, information or status).
Reading<BatteryState> ReadOneBattery(const std::wstring& path) {
    UniqueHandle device(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!device.valid()) {
        return Reading<BatteryState>::FromError(GetLastError());
    }
    auto tag = QueryTag(device.get());
    if (!tag.ok()) {
        return Reading<BatteryState>::Failure(tag.status, tag.error);
    }
    auto info = QueryInformation(device.get(), tag.value);
    if (!info.ok()) {
        return Reading<BatteryState>::Failure(info.status, info.error);
    }
    auto status = QueryStatus(device.get(), tag.value);
    if (!status.ok()) {
        return Reading<BatteryState>::Failure(status.status, status.error);
    }
    return Reading<BatteryState>::Success(BuildState(info.value, status.value));
}

/// The batteries read so far and the first failure.
struct Collected {
    std::vector<BatteryState> states;
    Status failStatus = Status::Ok;
    unsigned long failError = 0;
};

/// Reads one battery and adds it to the collection, or records the first failure.
/// @param path The device path.
/// @param collected The collection to extend.
void CollectBattery(const std::wstring& path, Collected& collected) {
    auto one = ReadOneBattery(path);
    if (one.ok()) {
        collected.states.push_back(one.value);
        return;
    }
    if (collected.failStatus == Status::Ok) {
        collected.failStatus = one.status;
        collected.failError = one.error;
    }
}

}  // namespace

Reading<std::vector<BatteryState>> SystemBatterySource::Read() {
    using Result = Reading<std::vector<BatteryState>>;
    PathList list = ListBatteryPaths();
    if (list.paths.empty() && list.error != 0) {
        return Result::FromError(list.error);
    }
    Collected collected;
    for (const std::wstring& path : list.paths) {
        CollectBattery(path, collected);
    }
    // An empty list with no failure means there is no battery: that is a known answer.
    if (collected.states.empty() && collected.failStatus != Status::Ok) {
        return Result::Failure(collected.failStatus, collected.failError);
    }
    return Result::Success(std::move(collected.states));
}

Reading<std::uint64_t> MilliwattHoursToMicrojoules(std::uint64_t milliwattHours) {
    if (milliwattHours > UINT64_MAX / kMicrojoulesPerMilliwattHour) {
        return Reading<std::uint64_t>::Failure(Status::Error, ERROR_ARITHMETIC_OVERFLOW);
    }
    return Reading<std::uint64_t>::Success(milliwattHours * kMicrojoulesPerMilliwattHour);
}

Reading<bool> ReadOnBattery() {
    SYSTEM_POWER_STATUS power{};
    if (!GetSystemPowerStatus(&power)) {
        return Reading<bool>::FromError(GetLastError());
    }
    if (power.ACLineStatus == 0) {
        return Reading<bool>::Success(true);
    }
    if (power.ACLineStatus == 1) {
        return Reading<bool>::Success(false);
    }
    return Reading<bool>::Failure(Status::Unavailable, 0);  // 255 = unknown, anything else is not documented
}

}  // namespace ews
