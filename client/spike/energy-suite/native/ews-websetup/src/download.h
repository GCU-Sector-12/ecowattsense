// download.h: one HTTPS download with WinHTTP for the web installer (SPEC 13).
// Used by steps.cpp for the manifest (into memory, no redirects) and for the package (into a file, redirects only to
// the allowed hosts). HTTPS only, the certificate is always checked, 10 second timeouts, at most `maxBytes` of body.
// Redirects are followed by hand: every target is checked with ews::IsAllowedRedirectUrl before it is requested.
#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace websetup {

/// How a download ended.
enum class DownloadOutcome {
    Ok,           ///< The whole body arrived.
    Network,      ///< Connection, certificate, timeout, a bad address or an HTTP status other than 200.
    BadRedirect,  ///< A redirect that is not allowed (not allowed at all, wrong host, not https, or too many).
    TooBig,       ///< The body is larger than maxBytes.
    Cancelled,    ///< The cancel flag was set.
    WriteFailed,  ///< The file could not be written.
    Redirect,     ///< Internal: the answer is a redirect to follow. Download() never returns it.
    More          ///< Internal: more body is expected. Download() never returns it.
};

/// Called as data arrives.
/// @param context The pointer from the request.
/// @param done Bytes received so far.
/// @param total The announced size, or 0 when unknown.
using ProgressFunction = void (*)(void* context, std::uint64_t done, std::uint64_t total);

/// The input and output of one download.
struct DownloadRequest {
    std::wstring url;                               ///< The first address (https).
    std::uint64_t maxBytes = 0;                     ///< The most body bytes accepted.
    bool allowRedirects = false;                    ///< False: any redirect is a failure.
    HANDLE file = INVALID_HANDLE_VALUE;             ///< When valid, the body is written here.
    std::string* memory = nullptr;                  ///< Otherwise the body is appended here.
    const std::atomic<bool>* cancel = nullptr;      ///< Checked before each chunk; may be null.
    ProgressFunction progress = nullptr;            ///< May be null.
    void* context = nullptr;                        ///< Given to progress.
    std::uint64_t received = 0;                     ///< Output: the body bytes that arrived.
};

/// Downloads one address.
/// @param request The request; `received` is set.
/// @return The outcome. Failures hold no address and no raw error text.
DownloadOutcome Download(DownloadRequest& request);

}  // namespace websetup
