// emi.h: Energy Meter Interface (EMI) reader: devices, channels, absolute energy samples, and the pure helpers
// (metadata parser, channel classifier, power from two samples, unit conversion).
// Used by ews-scan (source "emi") and ews-service (package and system power of each interval).
// No device path is stored in any result. Bytes from the driver are untrusted and bounds-checked.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ews/handles.h"
#include "ews/reading.h"

namespace ews {

/// What a channel measures (SPEC 3.3). Set from the channel name by ClassifyEmiChannel.
enum class EmiKind {
    System,      ///< The whole computer.
    CpuPackage,  ///< The processor package.
    Dram,        ///< Memory.
    CpuCores,    ///< Processor cores; part of the package.
    Igpu,        ///< Integrated graphics; part of the package.
    Other        ///< Anything else.
};

/// The unit of a channel's energy counter. EMI defines only picowatt-hours.
enum class EmiUnit {
    PicowattHours
};

/// A channel as the parser finds it in the metadata of one device.
struct EmiChannelMeta {
    std::string name;  ///< UTF-8, without terminator.
    EmiUnit unit = EmiUnit::PicowattHours;
};

/// One channel of one device. No device path is kept.
struct EmiChannel {
    std::uint32_t deviceIndex = 0;   ///< Position of the device in the enumeration.
    std::uint32_t channelIndex = 0;  ///< Position of the channel in that device.
    std::string name;                ///< UTF-8.
    EmiUnit unit = EmiUnit::PicowattHours;
    EmiKind kind = EmiKind::Other;
};

/// One reading of one channel. `known` is false when the read failed; then the numbers must not be used.
struct EmiSample {
    bool known = false;
    std::uint64_t energyPicowattHours = 0;  ///< Absolute energy counter.
    std::uint64_t time100ns = 0;            ///< Absolute time of the reading, in 100 ns units.
};

/// A source of EMI readings. Tests use a fake.
class IEmiSource {
public:
    virtual ~IEmiSource() = default;
    /// Finds the devices, opens them and reads their channels. Handles stay open for Read.
    /// @return Ok with every channel (an empty list when no EMI device exists), or the first failure
    ///         when devices exist but no channel could be read. A device that fails is skipped.
    virtual Reading<std::vector<EmiChannel>> Open() = 0;
    /// Reads the energy counter of every channel found by Open.
    /// @return Ok with one sample per channel, in the order Open returned them (a failed channel has known = false).
    ///         Unavailable when Open was not called.
    virtual Reading<std::vector<EmiSample>> Read() = 0;
};

/// The real source: SetupAPI with GUID_DEVICE_ENERGY_METER, then the four EMI IOCTLs.
/// Handles are opened with GENERIC_READ only and kept open between reads.
class SystemEmiSource final : public IEmiSource {
public:
    Reading<std::vector<EmiChannel>> Open() override;
    Reading<std::vector<EmiSample>> Read() override;

private:
    /// One open device.
    struct Device {
        UniqueHandle handle;
        std::uint16_t version = 0;
        std::size_t channelCount = 0;
    };
    std::vector<Device> devices_;
    bool opened_ = false;
};

/// Parses the metadata buffer of one EMI device.
/// @param version The EMI version (1 or 2) from IOCTL_EMI_GET_VERSION.
/// @param metadata The raw bytes from IOCTL_EMI_GET_METADATA.
/// @return Ok with the channels (one for version 1). Error (ERROR_INVALID_DATA) with no channels when the buffer is
///         shorter than the header, the channel count is 0 or above 256, a name size is 0, odd or runs past the
///         buffer, a name has no terminator, the unit is unknown, or the version is not 1 or 2.
Reading<std::vector<EmiChannelMeta>> ParseEmiMetadata(std::uint16_t version, std::span<const unsigned char> metadata);

/// Sets the kind of a channel from its name (SPEC 3.3), case-insensitive.
/// "system": PSYS, SYSTEM, PLATFORM or TOTAL is a whole word (words are split by '_' and ' ').
/// "cpu_package": RAPL_Package<N>_PKG, or exactly CPU. "dram": RAPL_Package<N>_DRAM.
/// "cpu_cores": RAPL_Package<N>_PP0, or a name starting with CPU_. "igpu": RAPL_Package<N>_PP1. Otherwise "other".
/// The system rule is tested first.
/// @param name The channel name (UTF-8).
/// @return The kind.
EmiKind ClassifyEmiChannel(const std::string& name);

/// @param kind A channel kind.
/// @return The fixed word of SPEC 3.3 (system, cpu_package, dram, cpu_cores, igpu, other).
const char* EmiKindWord(EmiKind kind);

/// Average power between two samples of one channel.
/// @param previous The earlier sample.
/// @param current The later sample.
/// @return Ok with watts. Unavailable when a sample is unknown, the time did not advance, or the energy counter went back.
Reading<double> EmiPowerWatts(const EmiSample& previous, const EmiSample& current);

/// Converts picowatt-hours to microjoules (1 pWh = 0.0036 uJ), rounded to the nearest whole number.
/// @param picowattHours The energy in pWh.
/// @return Ok with the energy in uJ. The result is always smaller than the input, so an overflow cannot happen;
///         the Reading form matches the other converters (see MilliwattHoursToMicrojoules).
Reading<std::uint64_t> PicowattHoursToMicrojoules(std::uint64_t picowattHours);

}  // namespace ews
