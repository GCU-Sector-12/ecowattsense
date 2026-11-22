// emi_kind.h: what an EMI channel measures (SPEC 3.3) and the rule that sets it from the channel name.
// Used by emi.h (channel list), the scanner (kind word in capabilities.json) and the service (which kinds are counted).
// No Windows header is needed here, so the rule is tested on any computer.
#pragma once

#include <string>

namespace ews {

/// What a channel measures (SPEC 3.3). Set from the channel name by ClassifyEmiChannel.
enum class EmiKind {
    System,      ///< The whole computer.
    CpuPackage,  ///< The processor package.
    Dram,        ///< Memory.
    CpuCores,    ///< Processor cores as one channel (RAPL PP0); part of the package.
    CpuCore,     ///< One physical core (RAPL_Package<N>_Core<M>_CORE, as AMD laptops show it); part of the package.
    Igpu,        ///< Integrated graphics; part of the package.
    Other        ///< Anything else.
};

/// Sets the kind of a channel from its name (SPEC 3.3), case-insensitive.
/// "system": PSYS, SYSTEM, PLATFORM or TOTAL is a whole word (words are split by '_' and ' ').
/// "cpu_package": RAPL_Package<N>_PKG, or exactly CPU. "dram": RAPL_Package<N>_DRAM.
/// "cpu_cores": RAPL_Package<N>_PP0, or a name starting with CPU_.
/// "cpu_core": RAPL_Package<N>_Core<M>_CORE. "igpu": RAPL_Package<N>_PP1. Otherwise "other".
/// The system rule is tested first.
/// @param name The channel name (UTF-8).
/// @return The kind.
EmiKind ClassifyEmiChannel(const std::string& name);

/// @param kind A channel kind.
/// @return The fixed word of SPEC 3.3 (system, cpu_package, dram, cpu_cores, cpu_core, igpu, other).
const char* EmiKindWord(EmiKind kind);

}  // namespace ews
