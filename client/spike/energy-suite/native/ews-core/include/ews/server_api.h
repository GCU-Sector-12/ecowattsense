// server_api.h: one call to the server for the ping, the settings and the device registration (SPEC 5.5).
// Used by ServiceCore (service_sync.cpp). The call sits behind an interface, so tests use a fake server.
// The real one is built on the same WinHTTP rules as the report sender: HTTPS only (HTTP only for loopback), the
// certificate is always checked, no redirects, 10 second timeouts, the answer is read up to 64 KB.
// The key and the URL are never logged or printed.
#pragma once

#include <string>

namespace ews {

inline constexpr int kMaxRetryAfterSeconds = 3600;  ///< The longest wait a `Retry-After` can ask for (one hour).

/// What came back from one call. `status` is 0 when there was no answer (connection, timeout or certificate failure).
struct ApiAnswer {
    int status = 0;
    std::string body;           ///< The start of the answer, up to 64 KB. Untrusted.
    int retryAfterSeconds = 0;  ///< The number of seconds of a `Retry-After` header; 0 when it is missing or not a whole number.
};

/// The fixed words that sort an HTTP answer. They are the words of `last_ping_result` in status.json (except Never).
enum class ApiWord {
    Ok,            ///< 200.
    Unauthorised,  ///< 401: the key is wrong.
    RateLimited,   ///< 429: wait `Retry-After`.
    ServerError,   ///< 503, any other 5xx, or an answer that is not understood.
    Network,       ///< No answer.
    Rejected       ///< 422 or another 4xx: a retry with the same body will not help.
};

/// @param word A word.
/// @return The fixed text: `ok`, `unauthorised`, `rate_limited`, `server_error`, `network` or `rejected`.
const char* ApiWordText(ApiWord word);

/// @param word A word.
/// @return A small whole number for the log: 0 ok, 1 unauthorised, 2 rate_limited, 3 server_error, 4 network, 5 rejected.
int ApiWordCode(ApiWord word);

/// Sorts an HTTP status code. A pure rule.
/// @param httpStatus The code, or 0 when there was no answer.
/// @return 200 gives Ok, 401 Unauthorised, 429 RateLimited, 0 Network, 5xx and every code not named here ServerError,
///         422 and the other 4xx Rejected.
ApiWord ApiWordFor(int httpStatus);

/// Reads the value of a `Retry-After` header.
/// @param text Untrusted header text.
/// @return The seconds when the text is one to nine digits only, else 0 (a date is not understood).
int ParseRetryAfter(const std::string& text);

/// @param answer An answer.
/// @return For 429 and 503 the `Retry-After` seconds limited to one hour, else 0.
int HoldSecondsFor(const ApiAnswer& answer);

/// The interface of a server call.
class ServerApi {
public:
    virtual ~ServerApi() = default;

    /// Makes one call.
    /// @param method `GET`, `POST` or `PUT`.
    /// @param path The path with the leading slash, for example `/api/ping`.
    /// @param body The JSON body; empty for a call without one.
    /// @return The answer. Never throws for a network failure (status 0).
    virtual ApiAnswer Call(const std::string& method, const std::string& path, const std::string& body) = 0;
};

/// The real server call, on WinHTTP.
class WinHttpServerApi final : public ServerApi {
public:
    /// @param serverUrl A URL that passed IsValidServerUrl (an invalid one makes every Call return status 0).
    /// @param serverKey A key that passed IsValidServerKey (an invalid one makes every Call return status 0).
    WinHttpServerApi(std::string serverUrl, std::string serverKey);
    ~WinHttpServerApi() override;
    WinHttpServerApi(const WinHttpServerApi&) = delete;
    WinHttpServerApi& operator=(const WinHttpServerApi&) = delete;

    /// Makes the call with the header `X-EWS-Key`.
    /// @param method `GET`, `POST` or `PUT`.
    /// @param path The path with the leading slash.
    /// @param body The JSON body; empty for none.
    /// @return The answer, or status 0 when there was none.
    ApiAnswer Call(const std::string& method, const std::string& path, const std::string& body) override;

private:
    std::string url_;
    std::string key_;
};

}  // namespace ews
