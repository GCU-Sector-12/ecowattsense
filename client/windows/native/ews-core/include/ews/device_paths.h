// device_paths.h: lists the device paths of one SetupAPI device interface class.
// Used by the battery and EMI readers. A path is only valid inside the caller; readers must not store it in a result.
#pragma once

#include <windows.h>

#include <cstddef>
#include <string>
#include <vector>

namespace ews {

/// The device paths found by an enumeration, and the first error met on the way (0 = none).
struct DevicePathList {
    std::vector<std::wstring> paths;
    unsigned long error = 0;
};

/// Lists the device paths of every present interface of one class.
/// @param interfaceClass The device interface class GUID.
/// @param maxCount The most paths to collect; a sane upper bound for the class.
/// @return The paths. error is 0 when the enumeration ended normally (also when no device exists),
///         otherwise the first Windows error code. A path that cannot be read sets error but does not stop the loop.
DevicePathList ListDeviceInterfacePaths(const GUID& interfaceClass, std::size_t maxCount);

}  // namespace ews
