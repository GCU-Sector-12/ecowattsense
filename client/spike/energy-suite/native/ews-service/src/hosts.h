// hosts.h: the three ways ews-service can run (SPEC 5.2). Used by main.cpp, which picks one from the command line.
#pragma once

#include "ews/service_args.h"

namespace ews_service {

/// `--configure`: reads at most 4 KB of `url=` and `key=` lines from standard input, writes service.ini and key.bin,
/// prints only the result word (ok, bad_url, bad_key, too_long or write_failed).
/// @param args The parsed command line (the data folder).
/// @return 0 for ok, else 1.
int RunConfigure(const ews::ServiceArgs& args);

/// `--console`: runs the loop in the console until Ctrl+C or until `--run-seconds` have passed.
/// @param args The parsed command line.
/// @return 0 after a clean stop; 4 when the service refused to start; 5 when the timer could not be made.
int RunConsole(const ews::ServiceArgs& args);

/// No argument: runs under the service control manager (StartServiceCtrlDispatcher).
/// @param args The parsed command line (only the data folder can differ from the default).
/// @return 0 after the service stopped; 1 when the program was not started by the service control manager.
int RunUnderScm(const ews::ServiceArgs& args);

}  // namespace ews_service
