// pawnio.h: detection of the PawnIO driver: installed (registry), its version, and whether its device opens here.
// Used by ews-scan (source "pawnio"). Detection only: no command is sent to the driver and no module is loaded.
// The result is logged only; it is not used for energy.
#pragma once

#include <optional>
#include <string>

#include "ews/reading.h"

namespace ews {

/// What the registry says about the PawnIO uninstall entry.
enum class PawnIoRegistry {
    Found,    ///< The value DisplayVersion exists (whatever its type or content).
    Missing,  ///< The key or the value does not exist.
    Unknown   ///< The registry could not be read (for example access denied).
};

/// What opening the PawnIO device gave.
enum class PawnIoDevice {
    Opened,    ///< The handle opened and was closed at once.
    Denied,    ///< Windows error 5: a standard user may not open it.
    NotFound,  ///< The device does not exist (error 2 or 3).
    Other      ///< Any other error; the code is in PawnIoState::deviceError.
};

/// Everything one read of the PawnIO source returns.
struct PawnIoState {
    PawnIoRegistry registry = PawnIoRegistry::Unknown;
    /// DisplayVersion as read: non-ASCII characters are '?', cut at 64 characters. Absent when the value is
    /// missing, empty or not a string. It is untrusted until IsValidPawnIoVersion says yes.
    std::optional<std::string> versionText;
    PawnIoDevice device = PawnIoDevice::Other;
    unsigned long deviceError = 0;  ///< Windows error code of the device open, 0 when it opened.
};

/// Checks a version text from the registry.
/// @param text The text as read.
/// @return True when it has 1 to 32 characters and all are digits or dots.
bool IsValidPawnIoVersion(const std::string& text);

/// A source of the PawnIO facts. Tests use a fake.
class IPawnIoSource {
public:
    virtual ~IPawnIoSource() = default;
    /// @return The state. A failed reading means the whole source is unknown; a registry or device problem is
    ///         reported inside the state instead.
    virtual Reading<PawnIoState> Read() = 0;
};

/// The real source: reads the uninstall entry (64-bit registry view, read-only) and opens the PawnIO device once.
class SystemPawnIoSource final : public IPawnIoSource {
public:
    Reading<PawnIoState> Read() override;
};

}  // namespace ews
