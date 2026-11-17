// test_http_sender.cpp: tests of the pure parts of the sender: the status code mapping, the body, the result words.
// The WinHTTP exchange itself is tested by scripts/Test-Sender.ps1 against a local listener.
#include <string>
#include <vector>

#include "ews/http_sender.h"
#include "test_common.h"

namespace {

/// Checks the result of InterpretResponse for one status code.
void CheckMapping(int httpStatus, const std::string& body, int batch, ews::SendWord word, bool leave, int rejected) {
    ews::SendResult result = ews::InterpretResponse(httpStatus, body, batch);
    bool same = result.word == word && result.leaveQueue == leave && result.rejected == rejected;
    if (!same) {
        ews_test::Check(false, ("mapping of status " + std::to_string(httpStatus)).c_str(), __FILE__, __LINE__);
    }
}

}  // namespace

EWS_TEST(SenderMapsStatusCodes) {
    const std::string none = "[]";
    CheckMapping(200, none, 5, ews::SendWord::Ok, true, 0);
    CheckMapping(201, none, 5, ews::SendWord::Ok, true, 0);
    CheckMapping(301, none, 5, ews::SendWord::ServerError, false, 0);
    CheckMapping(400, none, 5, ews::SendWord::Rejected, true, 5);
    CheckMapping(401, none, 5, ews::SendWord::Unauthorised, false, 0);
    CheckMapping(404, none, 5, ews::SendWord::Rejected, true, 5);
    CheckMapping(422, none, 5, ews::SendWord::Rejected, true, 5);
    CheckMapping(429, none, 5, ews::SendWord::ServerError, false, 0);
    CheckMapping(500, none, 5, ews::SendWord::ServerError, false, 0);
    CheckMapping(503, none, 5, ews::SendWord::ServerError, false, 0);
    CheckMapping(0, "", 5, ews::SendWord::Network, false, 0);
}

EWS_TEST(SenderMapsOddStatusCodesWithoutLosingReports) {
    CheckMapping(100, "", 3, ews::SendWord::ServerError, false, 0);
    CheckMapping(204, "", 3, ews::SendWord::ServerError, false, 0);
    CheckMapping(302, "", 3, ews::SendWord::ServerError, false, 0);
    CheckMapping(399, "", 3, ews::SendWord::ServerError, false, 0);
    CheckMapping(499, "", 3, ews::SendWord::Rejected, true, 3);
    CheckMapping(600, "", 3, ews::SendWord::ServerError, false, 0);
    CheckMapping(-1, "", 3, ews::SendWord::Network, false, 0);
}

EWS_TEST(SenderCountsRejectedItemsInTheAnswer) {
    const std::string answer = R"([{"status":"created"},{"status":"rejected","message":"x"},{"status":"duplicate"}])";
    CheckMapping(200, answer, 3, ews::SendWord::Ok, true, 1);
    const std::string three = R"([{"status":"rejected"},{"status":"rejected"},{"status":"rejected"}])";
    CheckMapping(200, three, 3, ews::SendWord::Ok, true, 3);
    CheckMapping(201, three, 2, ews::SendWord::Ok, true, 2);
    CheckMapping(200, "", 3, ews::SendWord::Ok, true, 0);
    CheckMapping(200, "rejected", 3, ews::SendWord::Ok, true, 0);
}

EWS_TEST(SenderBuildsTheBatchBody) {
    CHECK(ews::BuildBatchBody({}) == "[]");
    CHECK(ews::BuildBatchBody({"{\"a\":1}"}) == "[{\"a\":1}]");
    CHECK(ews::BuildBatchBody({"{\"a\":1}", "{\"b\":2}", "{}"}) == "[{\"a\":1},{\"b\":2},{}]");
}

EWS_TEST(SenderResultWords) {
    CHECK(std::string(ews::SendWordText(ews::SendWord::Ok)) == "ok");
    CHECK(std::string(ews::SendWordText(ews::SendWord::Unauthorised)) == "unauthorised");
    CHECK(std::string(ews::SendWordText(ews::SendWord::Rejected)) == "rejected");
    CHECK(std::string(ews::SendWordText(ews::SendWord::ServerError)) == "server_error");
    CHECK(std::string(ews::SendWordText(ews::SendWord::Network)) == "network");
}

EWS_TEST(SenderRefusesABadUrlOrKeyWithoutTouchingTheNetwork) {
    const std::vector<std::string> lines = {"{}"};
    ews::WinHttpSender badUrl("http://example.org", "key");
    CHECK(badUrl.Send(lines).word == ews::SendWord::Network);
    ews::WinHttpSender badKey("https://example.org", "bad\r\nkey");
    ews::SendResult result = badKey.Send(lines);
    CHECK(result.word == ews::SendWord::Network);
    CHECK(!result.leaveQueue);
    ews::WinHttpSender empty("https://example.org", "key");
    CHECK(empty.Send({}).word == ews::SendWord::Ok);
    CHECK(!empty.Send({}).leaveQueue);
}
