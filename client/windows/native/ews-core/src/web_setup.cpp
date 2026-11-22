// web_setup.cpp: implementation of the pure parts of the web installer (ews/web_setup.h, SPEC 13).
#include "ews/web_setup.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "ews/json_reader.h"

namespace ews {

namespace {

/// @param c A character.
/// @return True for 0-9, a-f and A-F.
bool IsHexChar(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

/// @param c A character.
/// @return True for a digit or a dot.
bool IsVersionChar(char c) { return (c >= '0' && c <= '9') || c == '.'; }

/// @param c A character.
/// @return True for printable ASCII without the space (0x21 to 0x7E).
bool IsPlainUrlChar(char c) { return c >= 0x21 && c <= 0x7E; }

/// @param c A character.
/// @return The lower-case form of an ASCII letter; other characters unchanged.
char LowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

/// @param value A JSON object.
/// @param key The member name.
/// @return The string text of the member, or null when the member is missing or not a string.
const std::string* StringMember(const JsonValue& value, const char* key) {
    const JsonValue* member = value.Find(key);
    if (member == nullptr || member->type != JsonType::String) {
        return nullptr;
    }
    return &member->text;
}

/// Reads the size member: a whole number from 1 to 300 MB.
/// @param value The manifest object.
/// @param size Receives the size.
/// @return True when valid.
bool ReadSizeMember(const JsonValue& value, std::uint64_t& size) {
    const JsonValue* member = value.Find("size");
    if (member == nullptr || member->type != JsonType::Number) {
        return false;
    }
    double number = member->number;
    bool whole = number == std::floor(number);
    bool inRange = number >= 1.0 && number <= static_cast<double>(kWebSetupMaxZipBytes);
    if (!whole || !inRange) {
        return false;
    }
    size = static_cast<std::uint64_t>(number);
    return true;
}

/// @param text A string.
/// @return True when it is not empty and holds only printable ASCII without spaces.
bool IsPlainUrl(std::string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), IsPlainUrlChar);
}

/// @param c A character.
/// @return True for a single or a double quote.
bool IsQuoteChar(char c) { return c == '"' || c == '\''; }

/// @param text A string.
/// @return The text in lower case.
std::string Lowered(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), LowerAscii);
    return result;
}

/// @param url The package address from the manifest.
/// @return True when it is plain, short enough and starts with the allowed prefix followed by a file path.
bool IsValidZipUrl(const std::string& url) {
    std::size_t prefixLength = std::strlen(kWebSetupZipUrlPrefix);
    bool shape = IsPlainUrl(url) && url.size() <= kWebSetupMaxUrlChars && url.size() > prefixLength;
    return shape && url.compare(0, prefixLength, kWebSetupZipUrlPrefix) == 0;
}

}  // namespace

bool IsValidServerUrl(std::string_view url) {
    constexpr std::string_view scheme = "https://";
    bool shape = IsPlainUrl(url) && url.size() > scheme.size() && url.size() <= 200 && url.substr(0, scheme.size()) == scheme;
    return shape && std::none_of(url.begin(), url.end(), IsQuoteChar);
}

Reading<WebManifest> ParseWebManifest(std::string_view text) {
    using Result = Reading<WebManifest>;
    Reading<JsonValue> json = ParseJson(text);
    if (!json.ok() || json.value.type != JsonType::Object) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    const std::string* version = StringMember(json.value, "version");
    const std::string* zipUrl = StringMember(json.value, "zip_url");
    const std::string* sha256 = StringMember(json.value, "sha256");
    WebManifest manifest;
    bool present = version != nullptr && zipUrl != nullptr && sha256 != nullptr && ReadSizeMember(json.value, manifest.size);
    if (!present) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    bool versionOk = !version->empty() && version->size() <= 32 && std::all_of(version->begin(), version->end(), IsVersionChar);
    bool hashOk = sha256->size() == 64 && std::all_of(sha256->begin(), sha256->end(), IsHexChar);
    if (!versionOk || !IsValidZipUrl(*zipUrl) || !hashOk) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    manifest.version = *version;
    manifest.zipUrl = *zipUrl;
    manifest.sha256 = Lowered(*sha256);
    const std::string* serverUrl = StringMember(json.value, "server_url");
    if (serverUrl != nullptr && IsValidServerUrl(*serverUrl)) {
        manifest.serverUrl = *serverUrl;
    }
    return Result::Success(manifest);
}

bool IsAllowedRedirectUrl(std::string_view url) {
    constexpr std::string_view scheme = "https://";
    bool plain = IsPlainUrl(url) && url.size() > scheme.size() && url.substr(0, scheme.size()) == scheme;
    if (!plain) {
        return false;
    }
    std::string_view rest = url.substr(scheme.size());
    std::size_t end = rest.find_first_of("/?#");
    std::string_view authority = end == std::string_view::npos ? rest : rest.substr(0, end);
    bool hasUserOrPort = authority.find_first_of("@:") != std::string_view::npos;
    if (authority.empty() || hasUserOrPort) {
        return false;
    }
    std::string host = Lowered(authority);
    return host == "github.com" || host == "objects.githubusercontent.com" || host == "release-assets.githubusercontent.com";
}

bool SizeMatches(std::uint64_t downloaded, std::uint64_t expected) { return downloaded == expected; }

bool Sha256Matches(std::string_view actualHex, std::string_view expectedHex) {
    bool shape = actualHex.size() == 64 && expectedHex.size() == 64 && std::all_of(actualHex.begin(), actualHex.end(), IsHexChar) &&
                 std::all_of(expectedHex.begin(), expectedHex.end(), IsHexChar);
    return shape && Lowered(actualHex) == Lowered(expectedHex);
}

int WebSetupExitCode(WebSetupResult result) { return static_cast<int>(result); }

const wchar_t* WebSetupMessage(WebSetupResult result) {
    switch (result) {
        case WebSetupResult::ManifestDownload:
            return L"Could not reach the download server. Check the internet connection and try again.";
        case WebSetupResult::ManifestInvalid:
            return L"The download information is not valid. Try again later.";
        case WebSetupResult::WorkFolder:
            return L"A temporary folder could not be created.";
        case WebSetupResult::PackageDownload:
            return L"The download failed. Check the internet connection and try again.";
        case WebSetupResult::SizeMismatch:
        case WebSetupResult::HashMismatch:
            return L"The download is damaged. Try again.";
        case WebSetupResult::HashFailed:
            return L"The download could not be checked. Try again.";
        case WebSetupResult::Extract:
            return L"The package could not be unpacked.";
        case WebSetupResult::SetupMissing:
            return L"The package does not contain the installer.";
        case WebSetupResult::NotStarted:
            return L"Setup was not started.";
        case WebSetupResult::Internal:
            return L"Setup could not start its work.";
        case WebSetupResult::SignatureRefused:
            return L"The downloaded Setup is not signed by EcoWattSense. It was not started.";
        case WebSetupResult::Ok:
        case WebSetupResult::Cancelled:
            return L"";
    }
    return L"";
}

}  // namespace ews
