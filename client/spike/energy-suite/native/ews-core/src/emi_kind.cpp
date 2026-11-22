// emi_kind.cpp: the channel classifier of the EMI reader (see ews/emi_kind.h).
// Used by emi.cpp (the real source), by ews-scan and ews-service, and by the tests.
// It uses no Windows header, so it is tested on any computer.

#include "ews/emi_kind.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace ews {

namespace {

/// @return The character in lower case (ASCII letters only; other bytes stay as they are).
char LowerChar(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

/// @return The text in lower case.
std::string ToLower(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), LowerChar);
    return lower;
}

/// Splits lower-case text into words at '_' and ' '. Digits do not split a word.
std::vector<std::string> SplitWords(const std::string& lower) {
    std::vector<std::string> words;
    std::size_t start = 0;
    while (start <= lower.size()) {
        std::size_t found = lower.find_first_of("_ ", start);
        std::size_t end = found == std::string::npos ? lower.size() : found;
        words.push_back(lower.substr(start, end - start));
        start = end + 1;
    }
    return words;
}

/// @return True when the lower-case word is one of the whole-computer words of SPEC 3.3.
bool IsSystemWord(const std::string& word) {
    return word == "psys" || word == "system" || word == "platform" || word == "total";
}

/// @return The part after "rapl_package<digits>_" of a lower-case name, or an empty string when the name has another shape.
std::string RaplSuffix(const std::string& lower) {
    const std::string prefix = "rapl_package";
    if (lower.compare(0, prefix.size(), prefix) != 0) {
        return std::string();
    }
    std::size_t digitsEnd = lower.find_first_not_of("0123456789", prefix.size());
    bool shapeOk = digitsEnd != std::string::npos && digitsEnd > prefix.size() && lower[digitsEnd] == '_';
    if (!shapeOk) {
        return std::string();
    }
    return lower.substr(digitsEnd + 1);
}

/// @return True when the suffix of a RAPL name has the shape "core<digits>_core" (one physical core).
bool IsCoreSuffix(const std::string& suffix) {
    const std::string head = "core";
    const std::string tail = "_core";
    if (suffix.compare(0, head.size(), head) != 0) {
        return false;
    }
    std::size_t digitsEnd = suffix.find_first_not_of("0123456789", head.size());
    bool shapeOk = digitsEnd != std::string::npos && digitsEnd > head.size();
    return shapeOk && suffix.compare(digitsEnd, std::string::npos, tail) == 0;
}

}  // namespace

EmiKind ClassifyEmiChannel(const std::string& name) {
    std::string lower = ToLower(name);
    std::vector<std::string> words = SplitWords(lower);
    if (std::any_of(words.begin(), words.end(), IsSystemWord)) {
        return EmiKind::System;
    }
    std::string suffix = RaplSuffix(lower);
    if (suffix == "pkg" || lower == "cpu") {
        return EmiKind::CpuPackage;
    }
    if (suffix == "dram") {
        return EmiKind::Dram;
    }
    if (IsCoreSuffix(suffix)) {
        return EmiKind::CpuCore;
    }
    if (suffix == "pp0" || lower.compare(0, 4, "cpu_") == 0) {
        return EmiKind::CpuCores;
    }
    if (suffix == "pp1") {
        return EmiKind::Igpu;
    }
    return EmiKind::Other;
}

const char* EmiKindWord(EmiKind kind) {
    switch (kind) {
        case EmiKind::System: return "system";
        case EmiKind::CpuPackage: return "cpu_package";
        case EmiKind::Dram: return "dram";
        case EmiKind::CpuCores: return "cpu_cores";
        case EmiKind::CpuCore: return "cpu_core";
        case EmiKind::Igpu: return "igpu";
        case EmiKind::Other: return "other";
    }
    return "other";
}

}  // namespace ews
