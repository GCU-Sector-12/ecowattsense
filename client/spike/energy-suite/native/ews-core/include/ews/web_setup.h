// web_setup.h: the pure and local parts of the web installer EcoWattSense-Web-Setup.exe (SPEC 13).
// Used by ews-websetup (the program) and by the tests. Nothing here touches the network:
// the manifest check, the address and redirect check, the size check, the SHA256 compare and the result words.
// Data from the network is untrusted: every manifest field is range-checked before use.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ews/reading.h"

namespace ews {

/// The only start of a package address the manifest may hold.
inline constexpr char kWebSetupZipUrlPrefix[] = "https://github.com/GCU-Sector-12/ecowattsense/releases/download/";

/// The largest package the installer accepts, in bytes (300 MB).
inline constexpr std::uint64_t kWebSetupMaxZipBytes = 300ull * 1024 * 1024;

/// The longest package address the installer accepts, in characters.
inline constexpr std::size_t kWebSetupMaxUrlChars = 512;

/// The checked content of latest.json.
struct WebManifest {
    std::string version;     ///< Digits and dots only.
    std::string zipUrl;      ///< Starts with kWebSetupZipUrlPrefix.
    std::string sha256;      ///< 64 lower-case hex digits.
    std::uint64_t size = 0;  ///< 1 to kWebSetupMaxZipBytes.
    std::string serverUrl;   ///< Optional "server_url": empty when it is missing or fails IsValidServerUrl.
};

/// Checks the optional server address of the manifest: starts with https://, 9 to 200 characters, printable ASCII,
/// no space and no quote (single or double).
/// @param url The text.
/// @return True when it may be given to Setup.exe as /server=<url>.
bool IsValidServerUrl(std::string_view url);

/// Reads and checks the manifest text.
/// @param text The whole latest.json (at most 64 KB; the JSON reader enforces that).
/// @return Ok with the manifest, or Error (ERROR_INVALID_DATA) for bad JSON, a missing or wrong-typed field, a version
///         with other characters than digits and dots, an address that does not start with kWebSetupZipUrlPrefix or holds
///         spaces, control or non-ASCII characters, a sha256 that is not 64 hex digits, or a size outside 1 to 300 MB.
Reading<WebManifest> ParseWebManifest(std::string_view text);

/// Tells whether a redirect target is allowed: https only, no user name, no port, and the host is one of
/// github.com, objects.githubusercontent.com or release-assets.githubusercontent.com (compared without case).
/// @param url The absolute address from the Location header.
/// @return True when the redirect may be followed.
bool IsAllowedRedirectUrl(std::string_view url);

/// @param downloaded The number of bytes that arrived.
/// @param expected The size from the manifest.
/// @return True when both are equal.
bool SizeMatches(std::uint64_t downloaded, std::uint64_t expected);

/// Compares two SHA256 values written as hex, without case.
/// @param actualHex The computed value.
/// @param expectedHex The value from the manifest.
/// @return True when both are 64 hex digits and equal.
bool Sha256Matches(std::string_view actualHex, std::string_view expectedHex);

/// Computes the SHA256 of a file with BCrypt, reading it in chunks.
/// @param path The file.
/// @return Ok with 64 lower-case hex digits, or a failed reading with the Windows error (file or BCrypt failure).
Reading<std::string> Sha256HexOfFile(const wchar_t* path);

/// The result of the installer. The exit code of the program says which step failed (see WebSetupExitCode).
enum class WebSetupResult {
    Ok,                ///< Setup.exe was started.
    Cancelled,         ///< The user pressed Cancel.
    ManifestDownload,  ///< The manifest could not be downloaded.
    ManifestInvalid,   ///< The manifest failed the check.
    WorkFolder,        ///< The temporary folder could not be created.
    PackageDownload,   ///< The package download failed, or was redirected to a host that is not allowed.
    SizeMismatch,      ///< The downloaded size differs from the manifest.
    HashMismatch,      ///< The SHA256 differs from the manifest.
    HashFailed,        ///< The file could not be read for the SHA256.
    Extract,           ///< tar.exe failed.
    SetupMissing,      ///< EcoWattSense\Setup.exe is not in the package.
    NotStarted,        ///< Setup.exe was not started (for example UAC was cancelled).
    Internal,          ///< The worker thread could not be started.
    SignatureRefused   ///< Setup.exe has no valid EcoWattSense signature; it was not started and the folder was deleted.
};

/// @param result A result.
/// @return The process exit code: 0 Ok, 1 Cancelled, 2 ManifestDownload, 3 ManifestInvalid, 4 WorkFolder,
///         5 PackageDownload, 6 SizeMismatch, 7 HashMismatch, 8 HashFailed, 9 Extract, 10 SetupMissing, 11 NotStarted, 12 Internal, 13 SignatureRefused.
int WebSetupExitCode(WebSetupResult result);

/// @param result A result.
/// @return The one line shown to the user. It holds no address and no raw error text. Empty for Ok and Cancelled.
const wchar_t* WebSetupMessage(WebSetupResult result);

}  // namespace ews
