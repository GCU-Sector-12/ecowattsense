// pdh_instance.h: one instance of a performance counter set with its value, as the PDH readers give it.
// Used by the ACPI power meter reader and the GPU Engine reader, and by the pure code that sums GPU use.
// No Windows header is needed here.
#pragma once

#include <string>

namespace ews {

/// One instance of a counter set.
struct PdhInstance {
    std::string name;    ///< Instance name, UTF-8.
    double value = 0.0;  ///< The counter value as PDH gives it. The unit is the counter's own; it is not converted here.
};

}  // namespace ews
