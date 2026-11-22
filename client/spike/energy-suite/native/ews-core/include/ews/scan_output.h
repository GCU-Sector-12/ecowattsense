// scan_output.h: turns a ScanResult into the text of capabilities.json and scan.log, and names the output files.
// Used by ews-scan (writes the files) and by the tests. The text is built by hand; numbers use a dot and no locale.
// The output never holds a serial number, user name, computer name or device path.
#pragma once

#include <string>
#include <vector>

#include "ews/scan.h"

namespace ews {

/// Builds the text of capabilities.json (SPEC 3.5): fixed key names and order, sources in the order
/// emi, battery, acpi_power_meter, display, gpu, gpu_use, intel_dtt, pawnio. A field with an unknown value is left out;
/// `live` unknown is null. Text is UTF-8; bytes that are not valid UTF-8 are replaced by '?'.
/// @param result The scan result.
/// @return The JSON text, ending with a newline.
std::string WriteCapabilitiesJson(const ScanResult& result);

/// @param machine The machine facts.
/// @return The machine object of capabilities.json as JSON text. Unknown fields are left out.
std::string MachineObjectJson(const MachineInfo& machine);

/// @param battery The battery result.
/// @return The battery source object of capabilities.json as JSON text.
std::string BatteryObjectJson(const BatteryResult& battery);

/// @param display The display result.
/// @return The display source object of capabilities.json as JSON text.
std::string DisplayObjectJson(const DisplayResult& display);

/// @param entries The NVML devices of the gpu source.
/// @return The list of devices as JSON text: [ { "name", "adapter", "power", "energy", "sample_w" }, ... ].
///         `adapter` and `sample_w` are left out when unknown. Used for the key of the list once the spec names it.
std::string NvmlEntriesJson(const std::vector<NvmlEntry>& entries);

/// Builds the text of scan.log: one line per step of each source (source id, step, result word, error code,
/// sample value), then one line each for level, calibration and device type.
/// @param result The scan result.
/// @return The log text, every line ending with a newline.
std::string WriteScanLog(const ScanResult& result);

/// @param context The scan context.
/// @return "capabilities.json", or "capabilities.service.json" for the service context.
const wchar_t* CapabilitiesFileName(ScanContext context);

/// @param context The scan context.
/// @return "scan.log", or "scan.service.log" for the service context.
const wchar_t* ScanLogFileName(ScanContext context);

}  // namespace ews
