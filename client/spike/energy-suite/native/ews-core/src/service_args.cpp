// service_args.cpp: the parser of the ews-service command line (see ews/service_args.h).
// Used by ews-service. It is a pure function; every argument is untrusted text.
#include "ews/service_args.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>

#include "ews/line_text.h"

namespace ews {

namespace {

constexpr std::size_t kBad = static_cast<std::size_t>(-1);
constexpr std::size_t kMaxDataDirLength = 1000;
constexpr int kMaxTickSeconds = 60;
constexpr int kMaxRunSeconds = 86400;

/// Which options were seen, so that a repeated one is refused.
struct Seen {
    bool mode = false;
    bool dataDir = false;
    bool tick = false;
    bool run = false;
};

/// The parser state: the result so far and the options seen.
struct ParseState {
    ServiceArgs args;
    Seen seen;
};

/// @return True when the text can serve as the value of an option: it is not empty and does not look like another option.
bool IsValue(const std::wstring& text) { return !text.empty() && !text.starts_with(L"--"); }

/// Reads the value of an option. @return The position of the value, or kBad when there is no usable value.
std::size_t ValueIndex(std::span<const std::wstring> arguments, std::size_t index) {
    bool usable = index + 1 < arguments.size() && IsValue(arguments[index + 1]);
    return usable ? index + 1 : kBad;
}

/// @return True for a plain ASCII digit.
bool IsAsciiDigit(wchar_t c) { return c >= L'0' && c <= L'9'; }

/// Reads a whole number written with ASCII digits only and checks the range.
/// @return The number, or nothing for any other text or a number outside min to max.
std::optional<int> ParseBounded(const std::wstring& text, int min, int max) {
    bool digitsOnly = std::all_of(text.begin(), text.end(), IsAsciiDigit);
    if (!digitsOnly) {
        return std::nullopt;
    }
    std::string narrow;
    std::transform(text.begin(), text.end(), std::back_inserter(narrow), [](wchar_t c) { return static_cast<char>(c); });
    std::optional<int> number = ParseWholeNumber(narrow);
    bool inRange = number.has_value() && *number >= min && *number <= max;
    return inRange ? number : std::nullopt;
}

/// Sets the mode once. @return The position after the option, or kBad when a mode was already given.
std::size_t ConsumeMode(ParseState& state, std::size_t index, ServiceMode mode) {
    if (state.seen.mode) {
        return kBad;
    }
    state.seen.mode = true;
    state.args.mode = mode;
    return index + 1;
}

/// Handles `--data-dir <folder>`. @return The position after the option, or kBad.
std::size_t ConsumeDataDir(std::span<const std::wstring> arguments, std::size_t index, ParseState& state) {
    std::size_t value = ValueIndex(arguments, index);
    bool usable = value != kBad && !state.seen.dataDir && arguments[value].size() <= kMaxDataDirLength;
    if (!usable) {
        return kBad;
    }
    state.seen.dataDir = true;
    state.args.dataDir = arguments[value];
    return value + 1;
}

/// Handles `--tick-seconds <n>`. @return The position after the option, or kBad.
std::size_t ConsumeTick(std::span<const std::wstring> arguments, std::size_t index, ParseState& state) {
    std::size_t value = ValueIndex(arguments, index);
    if (value == kBad || state.seen.tick) {
        return kBad;
    }
    std::optional<int> number = ParseBounded(arguments[value], 1, kMaxTickSeconds);
    state.seen.tick = true;
    state.args.tickSeconds = number.value_or(0);
    return number.has_value() ? value + 1 : kBad;
}

/// Handles `--run-seconds <n>`. @return The position after the option, or kBad.
std::size_t ConsumeRun(std::span<const std::wstring> arguments, std::size_t index, ParseState& state) {
    std::size_t value = ValueIndex(arguments, index);
    if (value == kBad || state.seen.run) {
        return kBad;
    }
    std::optional<int> number = ParseBounded(arguments[value], 1, kMaxRunSeconds);
    state.seen.run = true;
    state.args.runSeconds = number.value_or(0);
    return number.has_value() ? value + 1 : kBad;
}

/// Handles the argument at `index`. @return The position of the next argument, or kBad when this one is not allowed.
std::size_t ConsumeArgument(std::span<const std::wstring> arguments, std::size_t index, ParseState& state) {
    const std::wstring& argument = arguments[index];
    if (argument == L"--console") {
        return ConsumeMode(state, index, ServiceMode::Console);
    }
    if (argument == L"--configure") {
        return ConsumeMode(state, index, ServiceMode::Configure);
    }
    if (argument == L"--data-dir") {
        return ConsumeDataDir(arguments, index, state);
    }
    if (argument == L"--tick-seconds") {
        return ConsumeTick(arguments, index, state);
    }
    return argument == L"--run-seconds" ? ConsumeRun(arguments, index, state) : kBad;
}

/// @return True when the options that belong to console mode only appear in console mode.
bool ConsoleOptionsAllowed(const ParseState& state) {
    bool needsConsole = state.seen.tick || state.seen.run;
    return !needsConsole || state.args.mode == ServiceMode::Console;
}

}  // namespace

ServiceArgsResult ParseServiceArgs(std::span<const std::wstring> arguments) {
    ParseState state;
    std::size_t index = 0;
    while (index < arguments.size() && index != kBad) {
        index = ConsumeArgument(arguments, index, state);
    }
    ServiceArgsResult result;
    result.ok = index != kBad && ConsoleOptionsAllowed(state);
    result.args = result.ok ? state.args : ServiceArgs{};
    return result;
}

}  // namespace ews
