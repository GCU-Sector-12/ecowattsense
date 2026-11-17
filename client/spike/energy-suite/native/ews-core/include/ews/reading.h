// reading.h: the common result type of every reader in ews-core.
// Used by: all readers (processor times, battery, machine) and by ews-scan and ews-service.
// A reader never throws for a missing source. It returns a Reading with a status,
// the Windows error code and the value. A failed read means "unknown", never "does not exist".
#pragma once

#include <utility>

namespace ews {

/// The outcome of one read.
enum class Status {
    Ok,           ///< The value is valid.
    Unavailable,  ///< The source is not there, or has no value now.
    Denied,       ///< Windows refused access.
    Error         ///< Any other failure.
};

/// Maps a Windows error code to a status.
/// ERROR_ACCESS_DENIED gives Denied. ERROR_FILE_NOT_FOUND, ERROR_NOT_FOUND and ERROR_NO_MORE_ITEMS
/// give Unavailable. Every other code gives Error.
/// @param error A value from GetLastError().
/// @return The status for that code.
Status StatusFromWindowsError(unsigned long error);

/// The result of one read: status, Windows error code and value.
/// The value is only meaningful when status is Ok.
template <typename T>
struct Reading {
    Status status = Status::Unavailable;
    unsigned long error = 0;  ///< Windows error code, 0 when there was none.
    T value{};

    /// @return True when the value is valid.
    bool ok() const { return status == Status::Ok; }

    /// Builds a successful reading.
    /// @param v The value that was read.
    static Reading Success(T v) {
        Reading r;
        r.status = Status::Ok;
        r.value = std::move(v);
        return r;
    }

    /// Builds a failed reading with an explicit status.
    /// @param s The status (should not be Ok).
    /// @param e The Windows error code, or 0.
    static Reading Failure(Status s, unsigned long e) {
        Reading r;
        r.status = s;
        r.error = e;
        return r;
    }

    /// Builds a failed reading from a Windows error code; the status comes from StatusFromWindowsError.
    /// @param e A value from GetLastError().
    static Reading FromError(unsigned long e) { return Failure(StatusFromWindowsError(e), e); }
};

}  // namespace ews
