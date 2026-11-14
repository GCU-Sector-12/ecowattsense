// service_args.h: the command line of ews-service (SPEC 5.2), parsed by a pure function so it can be tested.
// Used by ews-service (main). Arguments are untrusted text from the user.
#pragma once

#include <span>
#include <string>

namespace ews {

/// What the program was asked to do.
enum class ServiceMode {
    Scm,        ///< No mode argument: run under the service control manager.
    Console,    ///< `--console`: the same loop in a console, for tests.
    Configure   ///< `--configure`: read `url=` and `key=` lines from standard input.
};

/// The default data folder.
inline constexpr const wchar_t* kDefaultDataDir = L"C:\\ProgramData\\EcoWattSense";

/// The parsed command line.
struct ServiceArgs {
    ServiceMode mode = ServiceMode::Scm;
    std::wstring dataDir = kDefaultDataDir;  ///< `--data-dir`, any mode.
    int tickSeconds = 60;                    ///< `--tick-seconds` 1 to 60, console only.
    int runSeconds = 0;                      ///< `--run-seconds` 1 to 86400, console only; 0 means no limit.
};

/// The outcome of the parser.
struct ServiceArgsResult {
    bool ok = false;  ///< False means "bad arguments" (exit code 3).
    ServiceArgs args; ///< Valid only when ok.
};

/// Parses `[--console | --configure] [--data-dir <folder>] [--tick-seconds <n>] [--run-seconds <n>]`.
/// `--tick-seconds` and `--run-seconds` need `--console`. An unknown argument, a repeated option, a missing or empty
/// value, a number that is not whole digits or is out of range, or both modes together gives ok = false.
/// @param arguments The arguments without the program name.
/// @return The parse result.
ServiceArgsResult ParseServiceArgs(std::span<const std::wstring> arguments);

}  // namespace ews
