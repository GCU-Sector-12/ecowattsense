// server_api.cpp: the pure rules of the server calls (see ews/server_api.h). The WinHTTP part is in http_sender.cpp.
// Used by ServiceCore. Nothing here touches the network.
#include "ews/server_api.h"

#include <algorithm>

namespace ews {

namespace {

/// @param c One character.
/// @return True for 0 to 9.
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

const char* ApiWordText(ApiWord word) {
    switch (word) {
        case ApiWord::Ok: return "ok";
        case ApiWord::Unauthorised: return "unauthorised";
        case ApiWord::RateLimited: return "rate_limited";
        case ApiWord::ServerError: return "server_error";
        case ApiWord::Network: return "network";
        case ApiWord::Rejected: return "rejected";
    }
    return "network";
}

int ApiWordCode(ApiWord word) { return static_cast<int>(word); }

ApiWord ApiWordFor(int httpStatus) {
    if (httpStatus == 200) {
        return ApiWord::Ok;
    }
    if (httpStatus == 401) {
        return ApiWord::Unauthorised;
    }
    if (httpStatus == 429) {
        return ApiWord::RateLimited;
    }
    if (httpStatus <= 0) {
        return ApiWord::Network;
    }
    bool clientError = httpStatus >= 400 && httpStatus <= 499;
    return clientError ? ApiWord::Rejected : ApiWord::ServerError;
}

int ParseRetryAfter(const std::string& text) {
    bool sized = !text.empty() && text.size() <= 9;
    bool digits = std::all_of(text.begin(), text.end(), IsDigit);
    return (sized && digits) ? std::stoi(text) : 0;
}

int HoldSecondsFor(const ApiAnswer& answer) {
    bool holds = answer.status == 429 || answer.status == 503;
    return holds ? std::clamp(answer.retryAfterSeconds, 0, kMaxRetryAfterSeconds) : 0;
}

}  // namespace ews
