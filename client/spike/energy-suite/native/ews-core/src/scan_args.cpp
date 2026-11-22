// scan_args.cpp: the parser of the ews-scan command line (see ews/scan_args.h).
// Used by ews-scan. It is a pure function; every argument is untrusted text.
#include "ews/scan_args.h"

#include <algorithm>
#include <cstddef>
#include <string>

namespace ews {

namespace {

constexpr std::size_t kBad = static_cast<std::size_t>(-1);
constexpr std::size_t kMaxPeriodDigits = 5;
constexpr std::uint32_t kMinPeriodMs = 500;
constexpr std::uint32_t kMaxPeriodMs = 10000;

/// The arguments so far, and what the combination rules need to know.
struct ParseState {
    ScanArgs args;
    bool periodGiven = false;
};

/// @return True when the character is a digit 0 to 9.
bool IsDigit(wchar_t c) { return c >= L'0' && c <= L'9'; }

/// @return True when the text is not empty and holds digits only.
bool AllDigits(const std::wstring& text) { return !text.empty() && std::all_of(text.begin(), text.end(), IsDigit); }

/// Reads the value of --period-ms.
/// @return The period in milliseconds, or nothing when the text has anything but one to five digits or the value is outside 500..10000.
std::optional<std::uint32_t> ParsePeriod(const std::wstring& text) {
    if (!AllDigits(text) || text.size() > kMaxPeriodDigits) {
        return std::nullopt;
    }
    std::uint32_t value = static_cast<std::uint32_t>(std::stoul(text));
    bool inRange = value >= kMinPeriodMs && value <= kMaxPeriodMs;
    return inRange ? std::optional<std::uint32_t>(value) : std::nullopt;
}

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

/// Handles `--period-ms <number>`. @return The position after the option, or kBad.
std::size_t ConsumePeriod(std::span<const std::wstring> arguments, std::size_t index, ParseState& state) {
    std::size_t value = ValueIndex(arguments, index);
    if (value == kBad) {
        return kBad;
    }
    auto period = ParsePeriod(arguments[value]);
    if (!period.has_value()) {
        return kBad;
    }
    state.args.periodMs = *period;
    state.periodGiven = true;
    return value + 1;
}

/// Handles the argument at `index`. @return The position of the next argument, or kBad when this one is not allowed.
std::size_t ConsumeArgument(std::span<const std::wstring> arguments, std::size_t index, ParseState& state) {
    const std::wstring& argument = arguments[index];
    if (argument == L"--quiet") {
        state.args.quiet = true;
        return index + 1;
    }
    if (argument == L"--watch") {
        state.args.watch = true;
        return index + 1;
    }
    if (argument == L"--out") {
        return ConsumeOut(arguments, index, state.args);
    }
    if (argument == L"--context") {
        return ConsumeContext(arguments, index, state.args);
    }
    if (argument == L"--period-ms") {
        return ConsumePeriod(arguments, index, state);
    }
    return kBad;
}

/// @return True when the options fit together: --period-ms needs --watch, and --watch excludes --out, --context and --quiet.
bool CombinationAllowed(const ParseState& state) {
    const ScanArgs& args = state.args;
    if (!args.watch) {
        return !state.periodGiven;
    }
    return args.outFolder.empty() && !args.context.has_value() && !args.quiet;
}

}  // namespace

ScanArgsResult ParseScanArgs(std::span<const std::wstring> arguments) {
    ParseState state;
    std::size_t index = 0;
    while (index < arguments.size() && index != kBad) {
        index = ConsumeArgument(arguments, index, state);
    }
    ScanArgsResult result;
    result.ok = index != kBad && CombinationAllowed(state);
    result.args = result.ok ? state.args : ScanArgs{};
    return result;
}

}  // namespace ews
