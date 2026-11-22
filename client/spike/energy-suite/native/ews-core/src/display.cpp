// display.cpp: display brightness reader (see ews/display.h).
// Used by ews-scan and ews-service. The device \\.\LCD exists only on computers with a built-in panel;
// opening it can fail with "file not found", which is a normal answer for a desktop.

#include <windows.h>
#include <winioctl.h>
#include <ntddvdeo.h>

#include "ews/display.h"
#include "ews/handles.h"

namespace ews {

namespace {

constexpr int kMaxPercent = 100;

}  // namespace

Reading<DisplayBrightness> SystemDisplaySource::Read() {
    using Result = Reading<DisplayBrightness>;
    UniqueHandle device(CreateFileW(L"\\\\.\\LCD", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!device.valid()) {
        return Result::FromError(GetLastError());
    }
    DISPLAY_BRIGHTNESS brightness{};
    DWORD returned = 0;
    if (!DeviceIoControl(device.get(), IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS, nullptr, 0, &brightness, sizeof(brightness), &returned,
                         nullptr)) {
        return Result::FromError(GetLastError());
    }
    if (returned < sizeof(brightness)) {
        return Result::Failure(Status::Error, ERROR_INSUFFICIENT_BUFFER);
    }
    return Result::Success(DisplayBrightness{brightness.ucACBrightness, brightness.ucDCBrightness});
}

Reading<int> BrightnessPercent(const DisplayBrightness& levels, bool onBattery) {
    int value = onBattery ? levels.onBattery : levels.onMains;
    if (value > kMaxPercent) {
        return Reading<int>::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    return Reading<int>::Success(value);
}

}  // namespace ews
