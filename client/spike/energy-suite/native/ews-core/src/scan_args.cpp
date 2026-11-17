// scan_args.cpp: the parser of the ews-scan command line (see ews/scan_args.h).
// Used by ews-scan. It is a pure function; every argument is untrusted text.
#include "ews/scan_args.h"

#include <cstddef>

namespace ews {

namespace {

constexpr std::size_t kBad = static_cast<std::size_t>(-1);

/// @return True when the text can serve as the value of an option: it is not empty and does not look like another option.
bool IsValue(const std::wstring& text) { return !text.empty() && !text.starts_with(L"--"); }

/// Reads the value of an option.
/// @param arguments All arguments.
/// @param index The position of the option.
/// @return The position of the value, or kBad when there is no usable value.
std::size_t ValueIndex(std::span<const std::wstring> arguments, std::size_t index) {
    bool usable = index + 1 < arguments.size() && IsValue(arguments[index + 1]);
    return usable ? index + 1 : kBad;
}

/// Maps the text of --context to a context.
/// @return The context, or nothing when the text is not user, admin or service (exact, lower case).
std::optional<ScanContext> ParseContext(const std::wstring& text) {
    if (text == L"user") {
        return ScanContext::User;
    }
    if (text == L"admin") {
        return ScanContext::Admin;
    }
    return text == L"service" ? std::optional<ScanContext>(ScanContext::Service) : std::nullopt;
}

/// Handles `--out <folder>`. @return The position after the option, or kBad.
std::size_t ConsumeOut(std::span<const std::wstring> arguments, std::size_t index, ScanArgs& parsed) {
    std::size_t value = ValueIndex(arguments, index);
    if (value == kBad) {
        return kBad;
    }
    parsed.outFolder = arguments[value];
    return value + 1;
}

/// Handles `--context <word>`. @return The position after the option, or kBad.
std::size_t ConsumeContext(std::span<const std::wstring> arguments, std::size_t index, ScanArgs& parsed) {
    std::size_t value = ValueIndex(arguments, index);
    if (value == kBad) {
        return kBad;
    }
    parsed.context = ParseContext(arguments[value]);
    return parsed.context.has_value() ? value + 1 : kBad;
}

/// Handles the argument at `index`. @return The position of the next argument, or kBad when this one is not allowed.
std::size_t ConsumeArgument(std::span<const std::wstring> arguments, std::size_t index, ScanArgs& parsed) {
    const std::wstring& argument = arguments[index];
    if (argument == L"--quiet") {
        parsed.quiet = true;
        return index + 1;
    }
    if (argument == L"--out") {
        return ConsumeOut(arguments, index, parsed);
    }
    if (argument == L"--context") {
        return ConsumeContext(arguments, index, parsed);
    }
    return kBad;
}

}  // namespace

ScanArgsResult ParseScanArgs(std::span<const std::wstring> arguments) {
    ScanArgs parsed;
    std::size_t index = 0;
    while (index < arguments.size() && index != kBad) {
        index = ConsumeArgument(arguments, index, parsed);
    }
    ScanArgsResult result;
    result.ok = index != kBad;
    result.args = result.ok ? parsed : ScanArgs{};
    return result;
}

}  // namespace ews
