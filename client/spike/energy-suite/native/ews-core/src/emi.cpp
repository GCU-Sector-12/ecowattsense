// emi.cpp: the real EMI source (see ews/emi.h): SetupAPI enumeration of GUID_DEVICE_ENERGY_METER, then the four
// EMI IOCTLs (version, metadata size, metadata, measurement) on handles that stay open between reads.
// Used by ews-scan and ews-service. The device path lives only inside Open; no path is kept in a result.
// Bytes from the driver are untrusted: the metadata goes through ParseEmiMetadata, and a measurement whose size
// differs from the channel count is treated as unknown for that device.

#include <windows.h>
#include <winioctl.h>
#include <emi.h>

#include <string>
#include <vector>

#include "ews/device_paths.h"
#include "ews/emi.h"

namespace ews {

namespace {

// GUID_DEVICE_ENERGY_METER, defined here to avoid the initguid.h include-order trap.
constexpr GUID kEmiInterface = {0x45bd8344, 0x7ed6, 0x49cf, {0xa4, 0x40, 0xc2, 0x76, 0xc9, 0x33, 0xb0, 0x53}};
constexpr std::size_t kMaxDevices = 16;             // a sane upper bound for one computer
constexpr ULONG kMaxMetadataBytes = 64 * 1024;      // a sane upper bound for one metadata buffer

/// A device that opened, answered the version and metadata calls and parsed.
struct OpenedDevice {
    UniqueHandle handle;
    std::uint16_t version = 0;
    std::vector<EmiChannelMeta> channels;
};

/// What Open has found so far: channels, devices and the first failure.
struct Collected {
    std::vector<EmiChannel> channels;
    std::vector<OpenedDevice> devices;
    Status failStatus = Status::Ok;
    unsigned long failError = 0;
};

/// Calls an EMI IOCTL that takes no input.
/// @param device An open EMI handle.
/// @param code The IOCTL code.
/// @param out The output buffer.
/// @param outSize The size of the output buffer in bytes.
/// @return Ok with the number of bytes the driver wrote, or the failure from Windows.
Reading<DWORD> Ioctl(HANDLE device, DWORD code, void* out, DWORD outSize) {
    DWORD returned = 0;
    if (!DeviceIoControl(device, code, nullptr, 0, out, outSize, &returned, nullptr)) {
        return Reading<DWORD>::FromError(GetLastError());
    }
    return Reading<DWORD>::Success(returned);
}

/// Reads the EMI version (IOCTL_EMI_GET_VERSION).
/// @return Ok with the version, or Error when the driver wrote a different number of bytes.
Reading<std::uint16_t> ReadVersion(HANDLE device) {
    EMI_VERSION version{};
    auto result = Ioctl(device, IOCTL_EMI_GET_VERSION, &version, sizeof(version));
    if (!result.ok()) {
        return Reading<std::uint16_t>::Failure(result.status, result.error);
    }
    if (result.value != sizeof(version)) {
        return Reading<std::uint16_t>::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    return Reading<std::uint16_t>::Success(version.EmiVersion);
}

/// Reads the metadata size (IOCTL_EMI_GET_METADATA_SIZE) and checks it against the upper bound.
/// @return Ok with the size in bytes, or Error when the driver gave 0 or a size above the bound.
Reading<ULONG> ReadMetadataSize(HANDLE device) {
    EMI_METADATA_SIZE size{};
    auto result = Ioctl(device, IOCTL_EMI_GET_METADATA_SIZE, &size, sizeof(size));
    if (!result.ok()) {
        return Reading<ULONG>::Failure(result.status, result.error);
    }
    bool sane = result.value == sizeof(size) && size.MetadataSize > 0 && size.MetadataSize <= kMaxMetadataBytes;
    if (!sane) {
        return Reading<ULONG>::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    return Reading<ULONG>::Success(size.MetadataSize);
}

/// Reads the metadata bytes (IOCTL_EMI_GET_METADATA).
/// @return Ok with the bytes the driver wrote, or the failure.
Reading<std::vector<unsigned char>> ReadMetadata(HANDLE device) {
    using Result = Reading<std::vector<unsigned char>>;
    auto size = ReadMetadataSize(device);
    if (!size.ok()) {
        return Result::Failure(size.status, size.error);
    }
    std::vector<unsigned char> buffer(size.value);
    auto result = Ioctl(device, IOCTL_EMI_GET_METADATA, buffer.data(), size.value);
    if (!result.ok()) {
        return Result::Failure(result.status, result.error);
    }
    buffer.resize(result.value <= size.value ? result.value : size.value);
    return Result::Success(std::move(buffer));
}

/// Opens one device and reads its version and metadata.
/// @param path The device path; it is not stored.
/// @return Ok with the open device, or the first failure (open, version, metadata or parse).
Reading<OpenedDevice> OpenOne(const std::wstring& path) {
    using Result = Reading<OpenedDevice>;
    OpenedDevice device;
    device.handle.Reset(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!device.handle.valid()) {
        return Result::FromError(GetLastError());
    }
    auto version = ReadVersion(device.handle.get());
    if (!version.ok()) {
        return Result::Failure(version.status, version.error);
    }
    auto metadata = ReadMetadata(device.handle.get());
    if (!metadata.ok()) {
        return Result::Failure(metadata.status, metadata.error);
    }
    auto parsed = ParseEmiMetadata(version.value, metadata.value);
    if (!parsed.ok()) {
        return Result::Failure(parsed.status, parsed.error);
    }
    device.version = version.value;
    device.channels = std::move(parsed.value);
    return Result::Success(std::move(device));
}

/// Adds the channels of an opened device to the collection.
/// @param deviceIndex The position of the device in the enumeration.
/// @param device The opened device.
/// @param collected The collection to extend.
void AppendChannels(std::uint32_t deviceIndex, const OpenedDevice& device, Collected& collected) {
    for (std::size_t i = 0; i < device.channels.size(); ++i) {
        EmiChannel channel;
        channel.deviceIndex = deviceIndex;
        channel.channelIndex = static_cast<std::uint32_t>(i);
        channel.name = device.channels[i].name;
        channel.unit = device.channels[i].unit;
        channel.kind = ClassifyEmiChannel(channel.name);
        collected.channels.push_back(std::move(channel));
    }
}

/// Opens one device and adds it to the collection, or records the first failure.
/// @param deviceIndex The position of the device in the enumeration.
/// @param path The device path.
/// @param collected The collection to extend.
void CollectDevice(std::uint32_t deviceIndex, const std::wstring& path, Collected& collected) {
    auto one = OpenOne(path);
    if (!one.ok()) {
        bool first = collected.failStatus == Status::Ok;
        collected.failStatus = first ? one.status : collected.failStatus;
        collected.failError = first ? one.error : collected.failError;
        return;
    }
    AppendChannels(deviceIndex, one.value, collected);
    collected.devices.push_back(std::move(one.value));
}

/// Turns the raw counters of one device into samples.
/// @param data The counters, one per channel.
/// @param samples Receives one known sample per counter; must have the same size.
void FillSamples(const std::vector<EMI_CHANNEL_MEASUREMENT_DATA>& data, std::vector<EmiSample>& samples) {
    for (std::size_t i = 0; i < data.size(); ++i) {
        samples[i] = EmiSample{true, data[i].AbsoluteEnergy, data[i].AbsoluteTime};
    }
}

/// Reads the measurement of one device (IOCTL_EMI_GET_MEASUREMENT).
/// @param device An open EMI handle.
/// @param channelCount The number of channels of the device (1 for version 1).
/// @return Exactly channelCount samples. All are unknown when the call fails or the driver wrote a number
///         of bytes that differs from the channel count.
std::vector<EmiSample> ReadMeasurement(HANDLE device, std::size_t channelCount) {
    std::vector<EmiSample> samples(channelCount);
    std::vector<EMI_CHANNEL_MEASUREMENT_DATA> data(channelCount);
    DWORD bytes = static_cast<DWORD>(channelCount * sizeof(EMI_CHANNEL_MEASUREMENT_DATA));
    auto result = Ioctl(device, IOCTL_EMI_GET_MEASUREMENT, data.data(), bytes);
    if (!result.ok() || result.value != bytes) {
        return samples;
    }
    FillSamples(data, samples);
    return samples;
}

}  // namespace

Reading<std::vector<EmiChannel>> SystemEmiSource::Open() {
    using Result = Reading<std::vector<EmiChannel>>;
    devices_.clear();
    opened_ = false;
    DevicePathList list = ListDeviceInterfacePaths(kEmiInterface, kMaxDevices);
    if (list.paths.empty() && list.error != 0) {
        return Result::FromError(list.error);
    }
    Collected collected;
    for (std::size_t i = 0; i < list.paths.size(); ++i) {
        CollectDevice(static_cast<std::uint32_t>(i), list.paths[i], collected);
    }
    // An empty list with no failure means there is no EMI device: that is a known answer.
    if (collected.channels.empty() && collected.failStatus != Status::Ok) {
        return Result::Failure(collected.failStatus, collected.failError);
    }
    for (OpenedDevice& opened : collected.devices) {
        devices_.push_back(Device{std::move(opened.handle), opened.version, opened.channels.size()});
    }
    opened_ = true;
    return Result::Success(std::move(collected.channels));
}

Reading<std::vector<EmiSample>> SystemEmiSource::Read() {
    using Result = Reading<std::vector<EmiSample>>;
    if (!opened_) {
        return Result::Failure(Status::Unavailable, 0);
    }
    std::vector<EmiSample> all;
    for (const Device& device : devices_) {
        std::vector<EmiSample> samples = ReadMeasurement(device.handle.get(), device.channelCount);
        all.insert(all.end(), samples.begin(), samples.end());
    }
    return Result::Success(std::move(all));
}

}  // namespace ews
