// steps.h: the work of the web installer, step by step (SPEC 13). Runs on the worker thread of main.cpp.
// The dialog is behind the SetupHost interface, so the steps do not know about windows.
#pragma once

#include <atomic>

#include "ews/web_setup.h"

namespace websetup {

/// What the steps need from the program: a cancel flag and a place to show progress.
class SetupHost {
public:
    virtual ~SetupHost() = default;

    /// @return The flag that is set when the user pressed Cancel.
    virtual const std::atomic<bool>& CancelFlag() const = 0;

    /// Shows one status line and the progress bar position. Called from the worker thread.
    /// @param line The status text.
    /// @param percent 0 to 100.
    virtual void Report(const wchar_t* line, int percent) = 0;
};

/// Runs the whole installer: manifest, download, SHA256 check, extraction, start of Setup.exe.
/// A failed step deletes the downloaded zip (and the folder when it is empty). A cancelled UAC prompt keeps the files.
/// @param host The cancel flag and the progress display.
/// @return Ok when Setup.exe was started, otherwise the step that failed.
ews::WebSetupResult RunWebSetup(SetupHost& host);

}  // namespace websetup
