// main.cpp: ews-service.exe, the service of the energy suite (SPEC 5).
// Used by the service control manager (no argument), by Setup and scripts (`--configure`) and by tests (`--console`).
// Usage: ews-service.exe [--console [--tick-seconds <1-60>] [--run-seconds <n>] | --configure] [--data-dir <folder>]
// Exit codes: 0 ok, 1 configure failed or not started by the service control manager, 3 bad arguments,
//             4 the service refused to start, 5 the timer could not be made.
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "ews/service_args.h"
#include "hosts.h"

namespace {

constexpr int kExitBadArguments = 3;

/// Prints the short usage text to standard error.
void PrintUsage() {
    std::fprintf(stderr, "usage: ews-service.exe [--console [--tick-seconds <1-60>] [--run-seconds <n>] | --configure] [--data-dir <folder>]\n");
}

/// Runs the mode the command line asked for.
int RunMode(const ews::ServiceArgs& args) {
    if (args.mode == ews::ServiceMode::Configure) {
        return ews_service::RunConfigure(args);
    }
    if (args.mode == ews::ServiceMode::Console) {
        return ews_service::RunConsole(args);
    }
    return ews_service::RunUnderScm(args);
}

}  // namespace

/// Program entry. @return The exit code of the chosen mode, or 3 for bad arguments.
int wmain(int argc, wchar_t** argv) {
    std::vector<std::wstring> arguments(argv + 1, argv + (argc > 0 ? argc : 1));
    ews::ServiceArgsResult parsed = ews::ParseServiceArgs(arguments);
    if (!parsed.ok) {
        PrintUsage();
        return kExitBadArguments;
    }
    return RunMode(parsed.args);
}
