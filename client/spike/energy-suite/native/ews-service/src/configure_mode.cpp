// configure_mode.cpp: the `--configure` mode of ews-service (SPEC 5.2).
// Used by Setup and by scripts. The key arrives on standard input, never on the command line. Only the result word is
// printed: no URL, no key and no part of the input.
#include <windows.h>

#include <cstdio>
#include <string>

#include "ews/configure.h"
#include "hosts.h"

namespace ews_service {

namespace {

constexpr DWORD kChunkBytes = 512;

/// Reads one chunk of standard input and appends it.
/// @return True when bytes were read; false at the end of the input or on a failed read.
bool AppendChunk(HANDLE input, std::string& text) {
    char chunk[kChunkBytes];
    DWORD bytes = 0;
    bool read = ReadFile(input, chunk, kChunkBytes, &bytes, nullptr) != 0;
    text.append(chunk, read ? bytes : 0);
    SecureZeroMemory(chunk, sizeof(chunk));
    return read && bytes > 0;
}

/// Reads standard input until its end, or until it is longer than the limit (then ApplyConfigureInput says too_long).
std::string ReadInput() {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    std::string text;
    while (text.size() <= ews::kMaxConfigureInputBytes && AppendChunk(input, text)) {
    }
    return text;
}

}  // namespace

int RunConfigure(const ews::ServiceArgs& args) {
    std::string input = ReadInput();
    ews::ConfigureResult result = ews::ApplyConfigureInput(input, args.dataDir);
    SecureZeroMemory(input.data(), input.size());
    std::printf("%s\n", ews::ConfigureResultWord(result));
    return result == ews::ConfigureResult::Ok ? 0 : 1;
}

}  // namespace ews_service
