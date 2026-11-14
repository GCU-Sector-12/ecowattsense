// main.cpp: ews-scan.exe, the scanner of the energy suite (SPEC 3).
// Used by Setup (once, with --out <data>\scan) and by the service (with --context service).
// It tests every energy source of this computer and writes capabilities.json and scan.log.
// Usage: ews-scan.exe [--out <folder>] [--context user|admin|service] [--quiet]
// Exit codes: 0 scan written, 2 output folder missing or not writable, 3 bad arguments.
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "ews/atomic_file.h"
#include "ews/battery.h"
#include "ews/display.h"
#include "ews/emi.h"
#include "ews/gpu.h"
#include "ews/handles.h"
#include "ews/intel_dtt.h"
#include "ews/pawnio.h"
#include "ews/version.h"
#include "ews/machine_info.h"
#include "ews/power_meter.h"
#include "ews/scan.h"
#include "ews/scan_args.h"
#include "ews/scan_output.h"

namespace {

constexpr int kExitWritten = 0;
constexpr int kExitOutputFolder = 2;
constexpr int kExitBadArguments = 3;

/// @return True when the process token is elevated (an administrator with the full token). A failed query counts as not elevated.
bool ProcessIsElevated() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw)) {
        return false;
    }
    ews::UniqueHandle token(raw);
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    if (!GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &size)) {
        return false;
    }
    return elevation.TokenIsElevated != 0;
}

/// @return The current UTC time as "YYYY-MM-DDTHH:MM:SSZ".
std::string NowUtcText() {
    SYSTEMTIME now{};
    GetSystemTime(&now);
    char text[32];
    std::snprintf(text, sizeof(text), "%04u-%02u-%02uT%02u:%02u:%02uZ", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
                  now.wSecond);
    return text;
}

/// @return True when the folder exists and is a folder.
bool FolderExists(const std::wstring& folder) {
    std::error_code ignored;
    return std::filesystem::is_directory(std::filesystem::path(folder), ignored);
}

/// Prints the result words of one source on one line.
void PrintSourceLine(const char* id, const ews::SourceCommon& c) {
    std::printf("%s: present=%s readable=%s live=%s error=%lu\n", id, ews::TriWord(c.present), ews::TriWord(c.readable),
                ews::TriWord(c.live), c.error);
}

/// Prints one short line per source, then level, calibration and device type.
void PrintSummary(const ews::ScanResult& result) {
    std::printf("ews-scan %s\n", ews::kSuiteVersion);
    PrintSourceLine("emi", result.emi.common);
    PrintSourceLine("battery", result.battery.common);
    PrintSourceLine("acpi_power_meter", result.powerMeter.common);
    PrintSourceLine("display", result.display.common);
    PrintSourceLine("gpu", result.gpu.common);
    PrintSourceLine("intel_dtt", result.intelDtt.common);
    PrintSourceLine("pawnio", result.pawnIo.common);
    std::printf("level: %s\ncalibration: %s\ndevice_type: %s\n", ews::LevelWord(result.level), ews::CalibrationWord(result.calibration),
                ews::DeviceTypeWord(result.deviceType));
}

/// Runs the scan with the real sources of this computer.
ews::ScanResult ScanThisComputer(ews::ScanContext context) {
    ews::SystemMachineInfoSource machine;
    ews::SystemEmiSource emi;
    ews::SystemBatterySource battery;
    ews::SystemPowerMeterSource powerMeter;
    ews::SystemDisplaySource display;
    ews::SystemGpuSource gpu;
    ews::SystemIntelDttSource intelDtt;
    ews::SystemPawnIoSource pawnIo;
    ews::SystemScanWaiter waiter;
    ews::ScanSources sources{machine, emi, battery, powerMeter, display, gpu, intelDtt, pawnIo, waiter, ews::ReadOnBattery};
    return ews::RunScan(sources, context, NowUtcText());
}

/// Writes capabilities.json and scan.log (each through a temporary file and a rename).
/// @return True when both files were written; otherwise prints the Windows error code to stderr.
bool WriteOutputs(const std::wstring& folder, const ews::ScanResult& result) {
    auto json = ews::WriteFileAtomically(folder, ews::CapabilitiesFileName(result.context), ews::WriteCapabilitiesJson(result));
    if (!json.ok()) {
        std::fprintf(stderr, "ews-scan: cannot write the capabilities file (error %lu)\n", json.error);
        return false;
    }
    auto log = ews::WriteFileAtomically(folder, ews::ScanLogFileName(result.context), ews::WriteScanLog(result));
    if (!log.ok()) {
        std::fprintf(stderr, "ews-scan: cannot write the log file (error %lu)\n", log.error);
    }
    return log.ok();
}

}  // namespace

/// Program entry. @return 0 when the scan was written, 2 for an output folder problem, 3 for bad arguments.
int wmain(int argc, wchar_t** argv) {
    std::vector<std::wstring> arguments(argv + 1, argv + (argc > 0 ? argc : 1));
    auto parsed = ews::ParseScanArgs(arguments);
    if (!parsed.ok) {
        std::fprintf(stderr, "usage: ews-scan.exe [--out <folder>] [--context user|admin|service] [--quiet]\n");
        return kExitBadArguments;
    }
    std::wstring folder = parsed.args.outFolder.empty() ? L"." : parsed.args.outFolder;
    if (!FolderExists(folder)) {
        std::fprintf(stderr, "ews-scan: the output folder does not exist\n");
        return kExitOutputFolder;
    }
    ews::ScanContext fallback = ProcessIsElevated() ? ews::ScanContext::Admin : ews::ScanContext::User;
    ews::ScanResult result = ScanThisComputer(parsed.args.context.value_or(fallback));
    if (!WriteOutputs(folder, result)) {
        return kExitOutputFolder;
    }
    if (!parsed.args.quiet) {
        PrintSummary(result);
    }
    return kExitWritten;
}
