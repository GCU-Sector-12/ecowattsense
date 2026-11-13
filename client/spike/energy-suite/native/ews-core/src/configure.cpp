// configure.cpp: the `--configure` input of the service (see ews/configure.h).
// Used by ews-service. The input is untrusted text: it is checked in full before anything is written,
// and nothing of it is returned or printed.
#include "ews/configure.h"

#include <windows.h>

#include "ews/config.h"
#include "ews/line_text.h"
#include "ews/secret.h"

namespace ews {

namespace {

/// The values found in the input.
struct ConfigureInput {
    bool hasUrl = false;
    bool hasKey = false;
    std::string url;
    std::string key;
};

/// Takes the value of one line when it starts with `url=` or `key=`; the last such line wins. Other lines are ignored.
void ReadLine(ConfigureInput& input, const std::string& line) {
    bool isUrl = line.rfind("url=", 0) == 0;
    bool isKey = line.rfind("key=", 0) == 0;
    input.hasUrl = input.hasUrl || isUrl;
    input.hasKey = input.hasKey || isKey;
    input.url = isUrl ? line.substr(4) : input.url;
    input.key = isKey ? line.substr(4) : input.key;
}

/// Checks the values and writes them. The key is written first, then the URL.
ConfigureResult Apply(const ConfigureInput& input, const std::wstring& dataDir) {
    if (!input.hasUrl && !input.hasKey) {
        return ConfigureResult::BadUrl;
    }
    if (input.hasUrl && !IsValidServerUrl(input.url)) {
        return ConfigureResult::BadUrl;
    }
    if (input.hasKey && !IsValidServerKey(input.key)) {
        return ConfigureResult::BadKey;
    }
    if (input.hasKey && !StoreServerKey(dataDir, input.key).ok()) {
        return ConfigureResult::WriteFailed;
    }
    if (input.hasUrl && !SaveServerUrl(dataDir, input.url).ok()) {
        return ConfigureResult::WriteFailed;
    }
    return ConfigureResult::Ok;
}

}  // namespace

const char* ConfigureResultWord(ConfigureResult result) {
    switch (result) {
        case ConfigureResult::Ok: return "ok";
        case ConfigureResult::BadUrl: return "bad_url";
        case ConfigureResult::BadKey: return "bad_key";
        case ConfigureResult::TooLong: return "too_long";
        case ConfigureResult::WriteFailed: return "write_failed";
    }
    return "write_failed";
}

ConfigureResult ApplyConfigureInput(const std::string& input, const std::wstring& dataDir) {
    if (input.size() > kMaxConfigureInputBytes) {
        return ConfigureResult::TooLong;
    }
    ConfigureInput parsed;
    for (const std::string& line : SplitLines(input)) {
        ReadLine(parsed, line);
    }
    ConfigureResult result = Apply(parsed, dataDir);
    // The copy of the key is wiped; the caller owns (and should wipe) the input text itself.
    SecureZeroMemory(parsed.key.data(), parsed.key.size());
    return result;
}

}  // namespace ews
