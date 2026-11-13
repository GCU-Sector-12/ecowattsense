// scan_output.cpp: the writers of capabilities.json and scan.log, and the output file names (see ews/scan_output.h).
// Used by ews-scan. The JSON is built by hand: one small function per object, so no library is needed.
// Numbers are written with std::to_chars, which ignores the locale and always uses a dot.
#include "ews/scan_output.h"

#include "ews/json_text.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ews {

namespace {

using Fields = std::vector<std::string>;

// ---- numbers ----

/// Formats a number with a dot, at most 3 decimals and at least one: 4.1, 12.0, 0.123.
/// @param value A finite number.
/// @return The text.
std::string FormatDouble(double value) {
    char buffer[64];
    auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, 3);
    std::string text(buffer, converted.ptr);
    while (text.size() > 2 && text.back() == '0' && text[text.size() - 2] != '.') {
        text.pop_back();
    }
    return text;
}

// ---- JSON pieces ----

/// @return `{ a, b }`, or `{ }` for no fields.
std::string Object(const Fields& fields) {
    std::string out = "{ ";
    for (std::size_t i = 0; i < fields.size(); ++i) {
        out += (i == 0 ? "" : ", ") + fields[i];
    }
    return out + (fields.empty() ? "}" : " }");
}

/// @return `[ a, b ]`, or `[ ]` for no items.
std::string Array(const std::vector<std::string>& items) {
    std::string out = "[ ";
    for (std::size_t i = 0; i < items.size(); ++i) {
        out += (i == 0 ? "" : ", ") + items[i];
    }
    return out + (items.empty() ? "]" : " ]");
}

/// Adds `"key": value` where value is already JSON text.
void AddRaw(Fields& fields, const char* key, const std::string& json) { fields.push_back("\"" + std::string(key) + "\": " + json); }

void AddText(Fields& fields, const char* key, const std::string& value) { AddRaw(fields, key, JsonQuote(value)); }

void AddWholeNumber(Fields& fields, const char* key, std::uint64_t value) { AddRaw(fields, key, std::to_string(value)); }

void AddBool(Fields& fields, const char* key, bool value) { AddRaw(fields, key, value ? "true" : "false"); }

/// Adds a text field only when the value is known.
void AddOptionalText(Fields& fields, const char* key, const std::optional<std::string>& value) {
    if (value.has_value()) {
        AddText(fields, key, *value);
    }
}

/// Adds a whole number field only when the value is known.
template <typename T>
void AddOptionalNumber(Fields& fields, const char* key, const std::optional<T>& value) {
    if (value.has_value()) {
        AddWholeNumber(fields, key, static_cast<std::uint64_t>(*value));
    }
}

/// Adds a decimal number field only when the value is known and finite.
void AddOptionalDecimal(Fields& fields, const char* key, const std::optional<double>& value) {
    if (value.has_value() && std::isfinite(*value)) {
        AddRaw(fields, key, FormatDouble(*value));
    }
}

/// Adds a bool field only when the value is known.
void AddOptionalBool(Fields& fields, const char* key, const std::optional<bool>& value) {
    if (value.has_value()) {
        AddBool(fields, key, *value);
    }
}

/// Adds a present / readable flag: left out when unknown.
void AddKnownFlag(Fields& fields, const char* key, Tri value) {
    if (value != Tri::Unknown) {
        AddBool(fields, key, value == Tri::True);
    }
}

/// Adds the live flag: true, false or null.
void AddLive(Fields& fields, Tri value) {
    std::string json = value == Tri::Unknown ? "null" : (value == Tri::True ? "true" : "false");
    AddRaw(fields, "live", json);
}

/// Starts the field list of one source with the id and the four common steps (error is always written).
Fields SourceFields(const char* id, const SourceCommon& common) {
    Fields fields;
    AddText(fields, "id", id);
    AddKnownFlag(fields, "present", common.present);
    AddKnownFlag(fields, "readable", common.readable);
    AddLive(fields, common.live);
    AddWholeNumber(fields, "error", common.error);
    return fields;
}

// ---- sources ----

std::string ChannelJson(const EmiChannelResult& channel) {
    Fields fields;
    AddWholeNumber(fields, "device", channel.device);
    AddWholeNumber(fields, "index", channel.index);
    AddText(fields, "name", channel.name);
    AddText(fields, "kind", EmiKindWord(channel.kind));
    AddLive(fields, channel.live);
    AddOptionalDecimal(fields, "sample_w", channel.sampleWatts);
    return Object(fields);
}

std::string EmiJson(const EmiResult& emi) {
    Fields fields = SourceFields("emi", emi.common);
    std::vector<std::string> channels;
    for (const auto& channel : emi.channels) {
        channels.push_back(ChannelJson(channel));
    }
    if (!channels.empty()) {
        AddRaw(fields, "channels", Array(channels));
    }
    return Object(fields);
}

std::string BatteryJson(const BatteryResult& battery) {
    Fields fields = SourceFields("battery", battery.common);
    AddOptionalNumber(fields, "count", battery.count);
    if (battery.unit.has_value()) {
        AddText(fields, "unit", *battery.unit == BatteryUnit::MilliwattHours ? "mWh" : "relative");
    }
    AddOptionalBool(fields, "rate_known", battery.rateKnown);
    AddOptionalNumber(fields, "full_mwh", battery.fullMwh);
    return Object(fields);
}

std::string PowerMeterJson(const PowerMeterResult& meter) {
    Fields fields = SourceFields("acpi_power_meter", meter.common);
    AddOptionalNumber(fields, "instances", meter.instances);
    AddOptionalDecimal(fields, "sample", meter.common.sample);
    return Object(fields);
}

std::string DisplayJson(const DisplayResult& display) {
    Fields fields = SourceFields("display", display.common);
    AddOptionalNumber(fields, "brightness_pct", display.brightnessPercent);
    return Object(fields);
}

std::string AdapterJson(const GpuAdapter& adapter) {
    Fields fields;
    AddText(fields, "name", adapter.description);
    AddWholeNumber(fields, "vendor_id", adapter.vendorId);
    AddWholeNumber(fields, "memory_mb", adapter.dedicatedMemoryMegabytes);
    AddBool(fields, "software", adapter.software);
    return Object(fields);
}

std::string GpuJson(const GpuResult& gpu) {
    Fields fields = SourceFields("gpu", gpu.common);
    std::vector<std::string> adapters;
    for (const auto& adapter : gpu.adapters) {
        adapters.push_back(AdapterJson(adapter));
    }
    if (gpu.common.readable == Tri::True) {
        AddRaw(fields, "adapters", Array(adapters));
    }
    if (gpu.nvml.has_value()) {
        AddText(fields, "nvml", NvmlWord(*gpu.nvml));
    }
    AddOptionalNumber(fields, "nvml_devices", gpu.nvmlDevices);
    return Object(fields);
}

std::string IntelDttJson(const IntelDttResult& dtt) { return Object(SourceFields("intel_dtt", dtt.common)); }

/// @return The machine object. Unknown fields are left out.
std::string MachineJson(const MachineInfo& machine) {
    Fields fields;
    AddOptionalText(fields, "maker", machine.smbios.maker);
    AddOptionalText(fields, "model", machine.smbios.model);
    if (machine.smbios.chassisType.has_value()) {
        AddText(fields, "chassis", ChassisWord(*machine.smbios.chassisType));
    }
    AddOptionalText(fields, "cpu_vendor", machine.processorVendor);
    AddOptionalText(fields, "cpu_name", machine.processorBrand);
    AddOptionalNumber(fields, "logical_processors", machine.logicalProcessors);
    AddOptionalNumber(fields, "windows_build", machine.windowsBuild);
    return Object(fields);
}

/// @return The six source objects, in the order of SPEC 3.5, one per entry.
std::vector<std::string> SourceLines(const ScanResult& result) {
    return {EmiJson(result.emi),         BatteryJson(result.battery), PowerMeterJson(result.powerMeter),
            DisplayJson(result.display), GpuJson(result.gpu),         IntelDttJson(result.intelDtt)};
}

/// @return The items joined by the separator.
std::string Join(const std::vector<std::string>& items, const std::string& separator) {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        out += (i == 0 ? "" : separator) + items[i];
    }
    return out;
}

// ---- log ----

/// @return The sample as text, or "-" when it is unknown.
std::string SampleText(const std::optional<double>& sample) {
    return (sample.has_value() && std::isfinite(*sample)) ? FormatDouble(*sample) : "-";
}

/// Appends one log line of one step.
void AddStepLine(std::string& log, const char* id, const char* step, const char* word, unsigned long error, const std::string& sample) {
    log += std::string(id) + " step=" + step + " result=" + word + " error=" + std::to_string(error) + " sample=" + sample + "\n";
}

/// Appends the four lines of one source: present, readable, live and sample.
void AddSourceLog(std::string& log, const char* id, const SourceCommon& common) {
    AddStepLine(log, id, "present", TriWord(common.present), common.error, "-");
    AddStepLine(log, id, "readable", TriWord(common.readable), common.error, "-");
    AddStepLine(log, id, "live", TriWord(common.live), common.error, "-");
    AddStepLine(log, id, "sample", common.sample.has_value() ? "ok" : "unknown", common.error, SampleText(common.sample));
}

}  // namespace

std::string WriteCapabilitiesJson(const ScanResult& result) {
    std::string out = "{\n";
    out += "  \"schema\": 1,\n";
    out += "  \"scanned_utc\": " + JsonQuote(result.scannedUtc) + ",\n";
    out += "  \"context\": " + JsonQuote(ContextWord(result.context)) + ",\n";
    out += "  \"machine\": " + MachineJson(result.machine) + ",\n";
    out += "  \"sources\": [\n    " + Join(SourceLines(result), ",\n    ") + "\n  ],\n";
    out += "  \"level\": " + JsonQuote(LevelWord(result.level)) + ",\n";
    out += "  \"calibration\": " + JsonQuote(CalibrationWord(result.calibration)) + ",\n";
    out += "  \"device_type\": " + JsonQuote(DeviceTypeWord(result.deviceType)) + "\n";
    out += "}\n";
    return out;
}

std::string WriteScanLog(const ScanResult& result) {
    std::string log;
    AddSourceLog(log, "emi", result.emi.common);
    AddSourceLog(log, "battery", result.battery.common);
    AddSourceLog(log, "acpi_power_meter", result.powerMeter.common);
    AddSourceLog(log, "display", result.display.common);
    AddSourceLog(log, "gpu", result.gpu.common);
    AddSourceLog(log, "intel_dtt", result.intelDtt.common);
    log += std::string("level ") + LevelWord(result.level) + "\n";
    log += std::string("calibration ") + CalibrationWord(result.calibration) + "\n";
    log += std::string("device_type ") + DeviceTypeWord(result.deviceType) + "\n";
    return log;
}

const wchar_t* CapabilitiesFileName(ScanContext context) {
    return context == ScanContext::Service ? L"capabilities.service.json" : L"capabilities.json";
}

const wchar_t* ScanLogFileName(ScanContext context) { return context == ScanContext::Service ? L"scan.service.log" : L"scan.log"; }

}  // namespace ews
