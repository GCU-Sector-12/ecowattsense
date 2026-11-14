// download.cpp: implementation of the WinHTTP download (download.h).
#include "download.h"

#include <vector>

#include "ews/text.h"
#include "ews/web_setup.h"
#include "winhttp_handle.h"

#pragma comment(lib, "winhttp.lib")

namespace websetup {

namespace {

constexpr DWORD kTimeoutMs = 10000;
constexpr int kMaxRedirects = 5;
constexpr DWORD kHostChars = 256;
constexpr DWORD kPathChars = 4096;
constexpr std::size_t kChunkBytes = 64 * 1024;

/// The pieces of an https address.
struct UrlParts {
    std::wstring host;
    std::wstring path;  ///< Path and query.
    INTERNET_PORT port = 0;
};

/// Splits an address with WinHttpCrackUrl.
/// @param url The address.
/// @param parts Receives host, path with query and port.
/// @return True for a well-formed https address that fits the buffers.
bool CrackHttps(const std::wstring& url, UrlParts& parts) {
    std::wstring host(kHostChars, L'\0');
    std::wstring path(kPathChars, L'\0');
    std::wstring extra(kPathChars, L'\0');
    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = host.data();
    components.dwHostNameLength = kHostChars;
    components.lpszUrlPath = path.data();
    components.dwUrlPathLength = kPathChars;
    components.lpszExtraInfo = extra.data();
    components.dwExtraInfoLength = kPathChars;
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        return false;
    }
    if (components.nScheme != INTERNET_SCHEME_HTTPS || components.dwHostNameLength == 0) {
        return false;
    }
    parts.host.assign(host, 0, components.dwHostNameLength);
    parts.path.assign(path, 0, components.dwUrlPathLength);
    parts.path.append(extra, 0, components.dwExtraInfoLength);
    parts.port = components.nPort;
    return true;
}

/// @param status An HTTP status code.
/// @return True for 301, 302, 303, 307 and 308.
bool IsRedirectStatus(DWORD status) { return status == 301 || status == 302 || status == 303 || status == 307 || status == 308; }

/// @param request A request whose response has arrived.
/// @return The HTTP status code, or 0 when it cannot be read.
DWORD QueryStatus(HINTERNET request) {
    DWORD status = 0;
    DWORD size = sizeof(status);
    DWORD flags = WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER;
    if (!WinHttpQueryHeaders(request, flags, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        return 0;
    }
    return status;
}

/// @param request A request whose response has arrived.
/// @return The announced body size, or 0 when there is none.
std::uint64_t QueryContentLength(HINTERNET request) {
    DWORD length = 0;
    DWORD size = sizeof(length);
    DWORD flags = WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER;
    if (!WinHttpQueryHeaders(request, flags, WINHTTP_HEADER_NAME_BY_INDEX, &length, &size, WINHTTP_NO_HEADER_INDEX)) {
        return 0;
    }
    return length;
}

/// Reads the Location header of a redirect and checks it.
/// @param request The request.
/// @param allowed False when redirects are not allowed at all.
/// @param next Receives the target address.
/// @return Redirect when the target may be followed, otherwise BadRedirect.
DownloadOutcome ReadRedirectTarget(HINTERNET request, bool allowed, std::wstring& next) {
    if (!allowed) {
        return DownloadOutcome::BadRedirect;
    }
    std::wstring location(kPathChars, L'\0');
    DWORD bytes = static_cast<DWORD>(location.size() * sizeof(wchar_t));
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) {
        return DownloadOutcome::BadRedirect;
    }
    location.resize(bytes / sizeof(wchar_t));
    std::string utf8 = ews::WideToUtf8(location.data(), location.size());
    if (!ews::IsAllowedRedirectUrl(utf8)) {
        return DownloadOutcome::BadRedirect;
    }
    next = location;
    return DownloadOutcome::Redirect;
}

/// Stores one chunk of the body in the file or in memory.
/// @param request The download request.
/// @param data The chunk.
/// @param size The chunk size.
/// @return False when the file write failed.
bool StoreChunk(DownloadRequest& request, const unsigned char* data, DWORD size) {
    if (request.file == INVALID_HANDLE_VALUE) {
        request.memory->append(reinterpret_cast<const char*>(data), size);
        return true;
    }
    DWORD written = 0;
    bool ok = WriteFile(request.file, data, size, &written, nullptr) != FALSE;
    return ok && written == size;
}

/// Reads and stores one chunk of the body.
/// @param request The download request.
/// @param http The request handle.
/// @param buffer A reusable buffer.
/// @param total The announced size, for the progress call.
/// @return More while data arrives, Ok at the end of the body, or a failure.
DownloadOutcome ReadStep(DownloadRequest& request, HINTERNET http, std::vector<unsigned char>& buffer, std::uint64_t total) {
    if (request.cancel != nullptr && request.cancel->load()) {
        return DownloadOutcome::Cancelled;
    }
    DWORD got = 0;
    if (!WinHttpReadData(http, buffer.data(), static_cast<DWORD>(buffer.size()), &got)) {
        return DownloadOutcome::Network;
    }
    if (got == 0) {
        return DownloadOutcome::Ok;
    }
    if (got > request.maxBytes - request.received) {
        return DownloadOutcome::TooBig;
    }
    if (!StoreChunk(request, buffer.data(), got)) {
        return DownloadOutcome::WriteFailed;
    }
    request.received += got;
    if (request.progress != nullptr) {
        request.progress(request.context, request.received, total);
    }
    return DownloadOutcome::More;
}

/// Reads the whole body of an answer with status 200.
/// @param request The download request.
/// @param http The request handle.
/// @return Ok or a failure.
DownloadOutcome ReadBody(DownloadRequest& request, HINTERNET http) {
    std::uint64_t total = QueryContentLength(http);
    if (total > request.maxBytes) {
        return DownloadOutcome::TooBig;
    }
    std::vector<unsigned char> buffer(kChunkBytes);
    DownloadOutcome outcome = DownloadOutcome::More;
    do {
        outcome = ReadStep(request, http, buffer, total);
    } while (outcome == DownloadOutcome::More);
    return outcome;
}

/// Makes one request and handles the answer: a body, a redirect, or a failure.
/// @param request The download request.
/// @param url The address of this hop.
/// @param next Receives the next address when the outcome is Redirect.
/// @return The outcome of this hop.
DownloadOutcome FetchOnce(DownloadRequest& request, const std::wstring& url, std::wstring& next) {
    UrlParts parts;
    if (!CrackHttps(url, parts)) {
        return DownloadOutcome::Network;
    }
    WinHttpHandle session(WinHttpOpen(L"EcoWattSense-Web-Setup", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.valid() || !WinHttpSetTimeouts(session.get(), kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs)) {
        return DownloadOutcome::Network;
    }
    WinHttpHandle connection(WinHttpConnect(session.get(), parts.host.c_str(), parts.port, 0));
    if (!connection.valid()) {
        return DownloadOutcome::Network;
    }
    WinHttpHandle http(WinHttpOpenRequest(connection.get(), L"GET", parts.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!http.valid() || !WinHttpSetOption(http.get(), WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy))) {
        return DownloadOutcome::Network;
    }
    bool answered = WinHttpSendRequest(http.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(http.get(), nullptr);
    if (!answered) {
        return DownloadOutcome::Network;
    }
    DWORD status = QueryStatus(http.get());
    if (IsRedirectStatus(status)) {
        return ReadRedirectTarget(http.get(), request.allowRedirects, next);
    }
    if (status != 200) {
        return DownloadOutcome::Network;
    }
    return ReadBody(request, http.get());
}

/// Fetches an address and follows allowed redirects (recursion keeps the code flat).
/// @param request The download request.
/// @param url The address.
/// @param depth How many redirects were followed already.
/// @return The final outcome.
DownloadOutcome Fetch(DownloadRequest& request, const std::wstring& url, int depth) {
    std::wstring next;
    DownloadOutcome outcome = FetchOnce(request, url, next);
    if (outcome != DownloadOutcome::Redirect) {
        return outcome;
    }
    if (depth >= kMaxRedirects) {
        return DownloadOutcome::BadRedirect;
    }
    return Fetch(request, next, depth + 1);
}

}  // namespace

DownloadOutcome Download(DownloadRequest& request) {
    request.received = 0;
    return Fetch(request, request.url, 0);
}

}  // namespace websetup
