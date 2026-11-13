// smbios.cpp: pure SMBIOS parser and chassis mapping (see ews/machine_info.h).
#include "ews/machine_info.h"

namespace ews {

std::span<const std::uint8_t> ExtractSmbiosTable(std::span<const std::uint8_t> raw) {
    return raw;
}

SmbiosInfo ParseSmbios(std::span<const std::uint8_t>) {
    SmbiosInfo info;
    info.maker = "wrong";
    return info;
}

const char* ChassisWord(std::uint8_t) {
    return "wrong";
}

}  // namespace ews
