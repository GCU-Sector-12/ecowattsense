// http_sender.h: sends a batch of reports to the server with WinHTTP (SPEC 5.5).
// Used by ews-service at each report tick. The sender sits behind an interface, so tests use a fake.
// `POST <server_url>/api/reports`, headers `X-EWS-Key` and `Content-Type: application/json`, 10 second timeouts,
// no redirects, the server certificate is always checked, the answer is read up to 64 KB.
// The key and the URL are never logged or printed.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ews {

inline constexpr std::size_t kMaxAnswerBytes = 64 * 1024;  ///< The most bytes of the answer that are read.

/// The fixed result words of one send. They are also the words of `last_send_result` in status.json.
enum class SendWord {
    Ok,            ///< The server took the batch (200 or 201).
    Unauthorised,  ///< 401: the key is wrong.
    Rejected,      ///< Another 4xx: the batch cannot be fixed by a retry.
    ServerError,   ///< 429, 5xx or any answer that is not understood: try again later.
    Network        ///< No answer: connection, timeout or certificate failure.
};

/// @param word A result word.
/// @return The fixed text: `ok`, `unauthorised`, `rejected`, `server_error` or `network`.
const char* SendWordText(SendWord word);

/// The result of one send.
struct SendResult {
    SendWord word = SendWord::Network;
    bool leaveQueue = false;  ///< True when the sent lines must leave the queue.
    int rejected = 0;         ///< How many reports the server refused (they left the queue too).
};

/// Turns an HTTP answer into the result. A pure rule.
/// 200 and 201: Ok, lines leave, rejected = the number of times the text `"rejected"` is in the body, at most `batchSize`.
/// 401: Unauthorised, lines stay. 429 and 5xx: ServerError, lines stay. Other 4xx: Rejected, lines leave,
/// rejected = `batchSize`. Code 0 (no answer): Network. Any other code (1xx, 3xx, other 2xx): ServerError, lines stay,
/// so nothing is lost on an answer that is not understood.
/// @param httpStatus The HTTP status code, or 0 when there was no answer.
/// @param body The start of the answer (up to 64 KB).
/// @param batchSize The number of lines that were sent.
/// @return The result.
SendResult InterpretResponse(int httpStatus, const std::string& body, int batchSize);

/// Builds the request body: `[` + the lines joined by `,` + `]`.
/// @param lines The report lines (each is one JSON object).
/// @return The body text.
std::string BuildBatchBody(const std::vector<std::string>& lines);

/// The interface of a sender.
class ReportSender {
public:
    virtual ~ReportSender() = default;

    /// Sends a batch.
    /// @param lines The report lines, up to 100.
    /// @return The result. Never throws for a network failure.
    virtual SendResult Send(const std::vector<std::string>& lines) = 0;
};

/// The real sender, on WinHTTP.
class WinHttpSender final : public ReportSender {
public:
    /// @param serverUrl A URL that passed IsValidServerUrl (an invalid one makes every Send return Network).
    /// @param serverKey A key that passed IsValidServerKey (an invalid one makes every Send return Network).
    WinHttpSender(std::string serverUrl, std::string serverKey);
    ~WinHttpSender() override;
    WinHttpSender(const WinHttpSender&) = delete;
    WinHttpSender& operator=(const WinHttpSender&) = delete;

    /// Sends the batch. An empty batch sends nothing and returns Ok with no lines leaving.
    /// @param lines The report lines.
    /// @return The result of the answer, or Network when there was none.
    SendResult Send(const std::vector<std::string>& lines) override;

private:
    std::string url_;
    std::string key_;
};

}  // namespace ews
