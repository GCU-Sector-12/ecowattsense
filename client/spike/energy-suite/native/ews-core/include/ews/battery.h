// battery.h: battery readers (capacity, rate, power state) and small helpers for them.
// Used by ews-scan (source "battery") and ews-service (energy on battery, on-battery flag).
// No device path, serial number or battery name is read into the result.
#pragma once

#include <cstdint>
#include <vector>

#include "ews/reading.h"

namespace ews {

/// The unit of the capacity fields of one battery.
enum class BatteryUnit {
    MilliwattHours,  ///< Capacities are in mWh.
    Relative         ///< Capacities are relative numbers (BATTERY_CAPACITY_RELATIVE); they are not energy.
};

/// One battery at one moment. A field that the driver marks unknown has its *Known flag false
/// and the value 0; callers must test the flag, never the value.
struct BatteryState {
    BatteryUnit unit = BatteryUnit::MilliwattHours;
    bool remainingKnown = false;
    std::uint32_t remaining = 0;     ///< Remaining capacity, in `unit`.
    bool fullChargedKnown = false;
    std::uint32_t fullCharged = 0;   ///< Full charged capacity, in `unit`.
    bool designedKnown = false;
    std::uint32_t designed = 0;      ///< Designed capacity, in `unit`.
    bool rateKnown = false;
    std::int32_t rate = 0;           ///< Rate in mW (relative units for Relative); negative while discharging.
    bool voltageKnown = false;
    std::uint32_t voltage = 0;       ///< Voltage in mV.
    bool onLine = false;             ///< Power state flag: mains is connected.
    bool charging = false;           ///< Power state flag: charging.
    bool discharging = false;        ///< Power state flag: discharging.
};

/// A source of battery readings. Tests use a fake.
class IBatterySource {
public:
    virtual ~IBatterySource() = default;
    /// Reads every battery.
    /// @return Ok with one state per battery (an empty list when there is no battery),
    ///         or the first failure when batteries exist but none could be read.
    virtual Reading<std::vector<BatteryState>> Read() = 0;
};

/// The real source: SetupAPI enumeration of battery device interfaces, then the battery IOCTLs
/// (query tag, query information, query status). Handles are opened for reading only.
class SystemBatterySource final : public IBatterySource {
public:
    Reading<std::vector<BatteryState>> Read() override;
};

/// Converts milliwatt-hours to microjoules (1 mWh = 3 600 000 uJ).
/// @param milliwattHours The energy in mWh.
/// @return Ok with the energy in uJ, or Error (ERROR_ARITHMETIC_OVERFLOW) when it does not fit in 64 bits.
Reading<std::uint64_t> MilliwattHoursToMicrojoules(std::uint64_t milliwattHours);

/// Tells whether the computer runs on battery now (GetSystemPowerStatus).
/// @return Ok with true for battery, false for mains. Unavailable when the AC line status is unknown (255).
Reading<bool> ReadOnBattery();

}  // namespace ews
