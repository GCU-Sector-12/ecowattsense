// http_sender.cpp: the status code rule and the WinHTTP sender (see ews/http_sender.h).
// Used by ews-service at each report tick. The key is only copied into the request header; it is never logged.
#include "ews/http_sender.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>

#include "ews/config.h"
#include "ews/secret.h"

#pragma comment(lib, "winhttp.lib")

namespace ews {

namespace {

constexpr DWORD kTimeoutMilliseconds = 10000;
constexpr DWORD kReadChunkBytes = 8192;

/// Owns one WinHTTP handle (session, connection or request) and closes it.
class InternetHandle {
public:
    InternetHandle() = default;
    explicit InternetHandle(HINTERNET handle) : handle_(handle) {}
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    ~InternetHandle() { Close(); }

    /// @return True when a handle is held.
    bool valid() const { return handle_ != nullptr; }
    /// @return The raw handle; the owner keeps it.
    HINTERNET get() const { return handle_; }

private:
    /// Closes the handle if there is one.
    void Close() {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }
    }

    HINTERNET handle_ = nullptr;
};

/// What came back from one exchange. `status` is 0 when there was no answer.
struct Answer {
    int status = 0;
    std::string body;
};

/// @return How many times `"rejected"` (with the quotes) is in the text.
int CountRejected(const std::string& body) {
    const std::string needle = "\"rejected\"";
    int count = 0;
    std::size_t position = body.find(needle);
    while (position != std::string::npos) {
        ++count;
        position = body.find(needle, position + needle.size());
    }
    return count;
}

/// Builds a result from its three parts.
SendResult Make(SendWord word, bool leave, int rejected) {
    SendResult result;
    result.word = word;
    result.leaveQueue = leave;
    result.rejected = rejected;
    return result;
}

/// @return True for the 4xx codes that mean "this batch can never be accepted" (all but 401 and 429).
bool IsPermanentClientError(int status) { return status >= 400 && status <= 499 && status != 401 && status != 429; }

/// @return True for 429 and 5xx.
bool IsRetryableError(int status) { return status == 429 || (status >= 500 && status <= 599); }

/// Widens ASCII text. The inputs were checked to be ASCII, so each byte is one character.
std::wstring Widen(const std::string& text) { return std::wstring(text.begin(), text.end()); }

/// Reads the next piece of the answer, up to the limit. @return True while more may be read.
bool ReadChunk(HINTERNET request, std::string& body) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request, &available) || available == 0) {
        return false;
    }
    std::string chunk(std::min(available, kReadChunkBytes), '\0');
    DWORD got = 0;
    if (!WinHttpReadData(request, chunk.data(), static_cast<DWORD>(chunk.size()), &got) || got == 0) {
        return false;
    }
    std::size_t room = kMaxAnswerBytes - body.size();
    body.append(chunk.data(), std::min<std::size_t>(got, room));
    return body.size() < kMaxAnswerBytes;
}

/// Reads the status code of the answer. @return The code, or 0 when it cannot be read.
int ReadStatusCode(HINTERNET request) {
    DWORD code = 0;
    DWORD size = sizeof code;
    BOOL done = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &size,
                                    WINHTTP_NO_HEADER_INDEX);
    return done ? static_cast<int>(code) : 0;
}

/// Sends the request and reads the answer. @return The answer; status 0 for any failure (no answer, timeout, bad certificate).
Answer ReadAnswer(HINTERNET request, const std::wstring& headers, std::string& body) {
    Answer answer;
    BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(headers.size()), body.data(), static_cast<DWORD>(body.size()),
                                   static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        return answer;
    }
    answer.status = ReadStatusCode(request);
    while (ReadChunk(request, answer.body)) {
    }
    return answer;
}

/// Makes the POST request to `/api/reports`: secure for https, redirects off, timeouts of 10 seconds.
/// The certificate is checked by WinHTTP; no security flag is set to ignore any error.
Answer Exchange(const ServerEndpoint& endpoint, const std::string& key, std::string& body) {
    InternetHandle session(WinHttpOpen(L"EcoWattSenseService/1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.valid() || !WinHttpSetTimeouts(session.get(), kTimeoutMilliseconds, kTimeoutMilliseconds, kTimeoutMilliseconds, kTimeoutMilliseconds)) {
        return Answer();
    }
    const std::string& host = endpoint.host;
    InternetHandle connection(WinHttpConnect(session.get(), Widen(host).c_str(), static_cast<INTERNET_PORT>(endpoint.port), 0));
    if (!connection.valid()) {
        return Answer();
    }
    DWORD flags = endpoint.secure ? WINHTTP_FLAG_SECURE : 0;
    InternetHandle request(WinHttpOpenRequest(connection.get(), L"POST", L"/api/reports", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!request.valid() || !WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof policy)) {
        return Answer();
    }
    std::wstring headers = L"X-EWS-Key: " + Widen(key) + L"\r\nContent-Type: application/json\r\n";
    return ReadAnswer(request.get(), headers, body);
}

}  // namespace

const char* SendWordText(SendWord word) {
    switch (word) {
        case SendWord::Ok: return "ok";
        case SendWord::Unauthorised: return "unauthorised";
        case SendWord::Rejected: return "rejected";
        case SendWord::ServerError: return "server_error";
        case SendWord::Network: return "network";
    }
    return "network";
}

SendResult InterpretResponse(int httpStatus, const std::string& body, int batchSize) {
    int batch = std::max(0, batchSize);
    if (httpStatus == 200 || httpStatus == 201) {
        return Make(SendWord::Ok, true, std::min(CountRejected(body), batch));
    }
    if (httpStatus == 401) {
        return Make(SendWord::Unauthorised, false, 0);
    }
    if (IsRetryableError(httpStatus)) {
        return Make(SendWord::ServerError, false, 0);
    }
    if (IsPermanentClientError(httpStatus)) {
        return Make(SendWord::Rejected, true, batch);
    }
    // Code 0 (or below) is "no answer". Any other code is not understood: keep the reports and try again.
    return httpStatus <= 0 ? Make(SendWord::Network, false, 0) : Make(SendWord::ServerError, false, 0);
}

std::string BuildBatchBody(const std::vector<std::string>& lines) {
    std::string body = "[";
    for (std::size_t index = 0; index < lines.size(); ++index) {
        body += index == 0 ? lines[index] : "," + lines[index];
    }
    body += "]";
    return body;
}

WinHttpSender::WinHttpSender(std::string serverUrl, std::string serverKey) : url_(std::move(serverUrl)), key_(std::move(serverKey)) {}

WinHttpSender::~WinHttpSender() { SecureZeroMemory(key_.data(), key_.size()); }

SendResult WinHttpSender::Send(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return Make(SendWord::Ok, false, 0);
    }
    std::optional<ServerEndpoint> endpoint = ParseServerUrl(url_);
    if (!endpoint.has_value() || !IsValidServerKey(key_)) {
        return Make(SendWord::Network, false, 0);
    }
    std::string body = BuildBatchBody(lines);
    Answer answer = Exchange(*endpoint, key_, body);
    return InterpretResponse(answer.status, answer.body, static_cast<int>(lines.size()));
}

}  // namespace ews
