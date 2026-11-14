// scan_args.h: the command line of ews-scan (SPEC 3.1), parsed by a pure function so it can be tested.
// Used by ews-scan (main). Arguments are untrusted text from the user.
#pragma once

#include <optional>
#include <span>
#include <string>

#include "ews/scan.h"

namespace ews {

/// The parsed command line of ews-scan.
struct ScanArgs {
    std::wstring outFolder;              ///< Empty means the current folder.
    std::optional<ScanContext> context;  ///< Empty means: decide from the process token.
    bool quiet = false;
};

/// The outcome of the parser.
struct ScanArgsResult {
    bool ok = false;  ///< False means "bad arguments" (exit code 3).
    ScanArgs args;    ///< Valid only when ok.
};

/// Parses `[--out <folder>] [--context user|admin|service] [--quiet]`.
/// A bad context, an unknown argument, or `--out` / `--context` without a value gives ok = false.
/// @param arguments The arguments without the program name.
/// @return The parse result.
ScanArgsResult ParseScanArgs(std::span<const std::wstring> arguments);

}  // namespace ews
