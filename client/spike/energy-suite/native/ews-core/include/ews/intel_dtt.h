// intel_dtt.h: presence of Intel Dynamic Tuning Technology (DTT), found by the service "esifsvc".
// Used by ews-scan (source "intel_dtt"). The result is logged only; it is not used for energy.
#pragma once

#include "ews/reading.h"

namespace ews {

/// A source of the DTT presence flag. Tests use a fake.
class IIntelDttSource {
public:
    virtual ~IIntelDttSource() = default;
    /// @return Ok with true when the service exists, Ok with false when Windows says it does not exist,
    ///         or the failure when the service manager cannot be asked.
    virtual Reading<bool> IsPresent() = 0;
};

/// The real source: asks the service control manager for the service "esifsvc" with query rights only.
class SystemIntelDttSource final : public IIntelDttSource {
public:
    Reading<bool> IsPresent() override;
};

}  // namespace ews
