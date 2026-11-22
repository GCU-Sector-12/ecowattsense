// diag_file.h: the real writer of the diagnostics file `<data>\logs\diag-<yyyy-MM-dd>.jsonl` (SPEC 7a).
// Used by ews-service (service_runtime.cpp), which gives it to ServiceCore. The rules (names, 7 days, 16 KB, counting)
// are in diag_recorder.h; this class only does the file work. UTF-8, one JSON object per line, appended, opened with
// shared read, written through to the disk after each line.
#pragma once

#include <string>

#include "ews/diag_recorder.h"

namespace ews {

/// Appends lines to the daily file below the data folder.
class DiagFileWriter final : public IDiagWriter {
public:
    /// @param dataDir The data folder (`<data>` of SPEC 2.2). The `logs` sub folder is made when it is missing.
    explicit DiagFileWriter(std::wstring dataDir);

    /// Appends one line and its line feed to the file of the UTC date of `time`, in one write.
    /// Full when the file would grow past kMaxDiagFileBytes. Failed when the folder or the file cannot be opened,
    /// or the write fails (disk full, sharing violation, access denied).
    DiagWriteResult Append(UtcTime time, const std::string& line) override;

    /// Deletes the files `diag-<date>.jsonl` that DiagFileIsOld names. Other files are never touched. A failure is ignored.
    void RemoveOld(UtcTime now) override;

private:
    std::wstring dataDir_;
};

}  // namespace ews
