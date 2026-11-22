// send_mode.cpp: the --send mode of ews-core-tests (see send_mode.h).
#include "send_mode.h"

#include <cstdio>
#include <string>
#include <vector>

#include "ews/config.h"
#include "ews/http_sender.h"

namespace ews_test {

int RunSend(const char* baseUrl) {
    std::string url = baseUrl;
    if (!ews::IsValidServerUrl(url)) {
        std::printf("bad_url\n");
        return 2;
    }
    ews::WinHttpSender sender(url, "test-key");
    const std::vector<std::string> lines = {R"({"n":1})", R"({"n":2})"};
    ews::SendResult result = sender.Send(lines);
    std::printf("%s leave=%d rejected=%d\n", ews::SendWordText(result.word), result.leaveQueue ? 1 : 0, result.rejected);
    return 0;
}

}  // namespace ews_test
