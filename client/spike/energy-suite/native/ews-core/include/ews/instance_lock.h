// instance_lock.h: the single-instance lock file `<data>\Client\instance.lock` (SPEC 5.3).
// Used by ews-service at start, so the old client mode and the service never report at the same time.
// It works like the lock of the old client: the file is opened with no sharing, so every other open fails
// while the service holds it. The file stays on disk after the release.
#pragma once

#include <string>

#include "ews/handles.h"

namespace ews {

/// The outcome of taking the lock.
enum class LockState {
    Taken,   ///< The service holds the lock now.
    Held,    ///< Another program holds it (sharing violation).
    Failed   ///< Any other failure (for example the folder cannot be made).
};

/// Holds the lock file open. Cannot be copied; the destructor releases the lock.
class InstanceLock {
public:
    /// Makes `<data>\Client` when it is missing and opens `instance.lock` with no sharing.
    /// @param dataDir The data folder; it must exist.
    /// @return Taken, Held (only for a sharing violation) or Failed. `error` holds the Windows error code of a failure.
    LockState Acquire(const std::wstring& dataDir);

    /// Closes the file and so releases the lock. Safe to call when nothing is held.
    void Release();

    /// @return The Windows error code of the last failed Acquire, 0 when there was none.
    unsigned long error() const { return error_; }

private:
    UniqueHandle file_;
    unsigned long error_ = 0;
};

}  // namespace ews
