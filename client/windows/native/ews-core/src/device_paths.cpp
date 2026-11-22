// device_paths.cpp: SetupAPI enumeration of one device interface class (see ews/device_paths.h).
// Used by the battery and EMI readers. Moved out of battery.cpp so both share one tested loop.

#include "ews/device_paths.h"

#include <setupapi.h>

#include "ews/handles.h"

#pragma comment(lib, "setupapi.lib")

namespace ews {

namespace {

/// Reads the device path of one enumerated interface and appends it to the list.
/// @param devices The device information set.
/// @param data The interface returned by SetupDiEnumDeviceInterfaces.
/// @param list The list to extend; list.error is set when the path cannot be read.
void AppendPath(HDEVINFO devices, SP_DEVICE_INTERFACE_DATA& data, DevicePathList& list) {
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

}  // namespace

DevicePathList ListDeviceInterfacePaths(const GUID& interfaceClass, std::size_t maxCount) {
    DevicePathList list;
    UniqueDevInfo devices(SetupDiGetClassDevsW(&interfaceClass, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (!devices.valid()) {
        list.error = GetLastError();
        return list;
    }
    SP_DEVICE_INTERFACE_DATA data{};
    data.cbSize = sizeof(data);
    DWORD index = 0;
    while (list.paths.size() < maxCount && SetupDiEnumDeviceInterfaces(devices.get(), nullptr, &interfaceClass, index, &data)) {
        AppendPath(devices.get(), data, list);
        ++index;
    }
    DWORD last = GetLastError();
    bool endedNormally = last == ERROR_NO_MORE_ITEMS || list.paths.size() >= maxCount;
    list.error = list.error != 0 ? list.error : (endedNormally ? 0 : last);
    return list;
}

}  // namespace ews
