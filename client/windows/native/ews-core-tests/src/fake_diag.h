// fake_diag.h: a fake diagnostics writer for the tests (ews/diag_recorder.h).
// Used by test_diag_recorder.cpp and by the service tests (service_rig.h, test_service_diag.cpp). It keeps the lines
// that were written, so a test can check which lines a tick, a report, a send or an event produced. No file is touched.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ews/diag_recorder.h"

namespace ews_test {

/// Keeps every line it is given. `next` is the result of the following Append calls.
class FakeDiagWriter final : public ews::IDiagWriter {
public:
    std::vector<std::string> lines;       ///< The lines that were written (the result was Written).
    std::vector<ews::UtcTime> times;      ///< The time of each written line.
    ews::DiagWriteResult next = ews::DiagWriteResult::Written;
    int appends = 0;                      ///< How many times Append was called, whatever the result.
    int cleans = 0;                       ///< How many times RemoveOld was called.
    ews::UtcTime lastClean{};

    ews::DiagWriteResult Append(ews::UtcTime time, const std::string& line) override {
        ++appends;
        if (next != ews::DiagWriteResult::Written) {
            return next;
        }
        lines.push_back(line);
        times.push_back(time);
        return next;
    }

    void RemoveOld(ews::UtcTime now) override {
        ++cleans;
        lastClean = now;
    }

    /// @param line A line. @param prefix A text. @return True when the line starts with the prefix.
    static bool StartsWith(const std::string& line, const std::string& prefix) { return line.compare(0, prefix.size(), prefix) == 0; }

    /// @param prefix The start of a line, for example `{"t":"tick"`.
    /// @return How many written lines start with it.
    int CountStarting(const std::string& prefix) const {
        int count = 0;
        for (const std::string& line : lines) {
            count += StartsWith(line, prefix) ? 1 : 0;
        }
        return count;
    }

    /// @param prefix The start of a line.
    /// @return The first written line that starts with it, or an empty text.
    std::string FirstStarting(const std::string& prefix) const {
        std::string found;
        for (const std::string& line : lines) {
            found = (found.empty() && StartsWith(line, prefix)) ? line : found;
        }
        return found;
    }

    /// @param part A piece of text.
    /// @return How many written lines contain it.
    int CountContaining(const std::string& part) const {
        int count = 0;
        for (const std::string& line : lines) {
            count += line.find(part) != std::string::npos ? 1 : 0;
        }
        return count;
    }
};

}  // namespace ews_test
