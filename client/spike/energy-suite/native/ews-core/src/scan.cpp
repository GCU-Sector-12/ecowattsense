// scan.cpp: the pure decision functions of SPEC 3.4 and the fixed words of the scan result (see ews/scan.h).
// Used by RunScan, the JSON and log writers, and ews-scan.
#include <string_view>

#include "ews/scan.h"

namespace ews {

MeasurementLevel DecideLevel(const LevelFacts& facts) {
    if (facts.liveSystemChannel || facts.livePowerMeter) {
        return MeasurementLevel::L1;
    }
    if (facts.liveCpuPackage) {
        return MeasurementLevel::L2;
    }
    if (facts.mwhBattery) {
        return MeasurementLevel::L3;
    }
    return MeasurementLevel::L4;
}

Calibration DecideCalibration(MeasurementLevel level, bool mwhBattery) {
    if (level == MeasurementLevel::L1) {
        return Calibration::NotNeeded;
    }
    if (level == MeasurementLevel::L4) {
        return Calibration::NotPossible;
    }
    return mwhBattery ? Calibration::Needed : Calibration::NotPossible;
}

DeviceType DecideDeviceType(std::optional<std::uint8_t> chassisType, bool batteryPresent) {
    if (batteryPresent) {
        return DeviceType::Laptop;
    }
    if (!chassisType.has_value()) {
        return DeviceType::Desktop;
    }
    // ChassisWord ignores bit 7 and gives "laptop" for the portable chassis types of SMBIOS.
    bool portable = std::string_view(ChassisWord(*chassisType)) == "laptop";
    return portable ? DeviceType::Laptop : DeviceType::Desktop;
}

const char* TriWord(Tri value) {
    switch (value) {
        case Tri::True: return "yes";
        case Tri::False: return "no";
        default: return "unknown";
    }
}

const char* ContextWord(ScanContext context) {
    switch (context) {
        case ScanContext::Admin: return "admin";
        case ScanContext::Service: return "service";
        default: return "user";
    }
}

const char* LevelWord(MeasurementLevel level) {
    switch (level) {
        case MeasurementLevel::L1: return "L1";
        case MeasurementLevel::L2: return "L2";
        case MeasurementLevel::L3: return "L3";
        default: return "L4";
    }
}

const char* CalibrationWord(Calibration calibration) {
    switch (calibration) {
        case Calibration::NotNeeded: return "not_needed";
        case Calibration::Needed: return "needed";
        default: return "not_possible";
    }
}

const char* DeviceTypeWord(DeviceType type) { return type == DeviceType::Laptop ? "laptop" : "desktop"; }

const char* NvmlWord(NvmlState state) {
    switch (state) {
        case NvmlState::Ok: return "ok";
        case NvmlState::Unavailable: return "unavailable";
        default: return "error";
    }
}

}  // namespace ews
