// test_rename_service.cpp: tests of the rename request on the session pipe (`EWS1 name=<base64>`), the name file,
// the PUT to the server, the pending state and the three status fields (SPEC 5.5, 5.7 and 6) of ServiceCore.
// The fake server (FakeSenders) records every call and answers from a script.
#include <filesystem>
#include <string>
#include <vector>

#include "ews/device_identity.h"
#include "service_rig.h"
#include "test_common.h"

using ews_test::FakeSenders;
using ews_test::Has;
using ews_test::Rig;

namespace {

constexpr const char* kLabPc = "EWS1 name=TGFiIFBD\n";  // "Lab PC"
constexpr const char* kCafe = "EWS1 name=Q2Fmw6k=\n";   // "Cafe" with an acute e

/// Starts the rig and opens the pipe path (somebody is logged on).
void StartOk(Rig& rig) {
    CHECK(rig.core.Start() == ews::StartResult::Ok);
    rig.core.SessionsChanged(true, false);
}

/// @return The device id of the rig.
std::string DeviceId(Rig& rig) { return ews::LoadOrCreateDeviceIdentity(rig.data.wide()).value.id; }

/// @return The path of the PUT of the rig.
std::string PutPath(Rig& rig) { return "/api/devices/" + DeviceId(rig); }

/// @return The text of Client\device-name.txt, or "<none>" when the file is missing.
std::string NameFile(Rig& rig) {
    std::filesystem::path file = rig.data.path() / "Client" / "device-name.txt";
    return std::filesystem::exists(file) ? ews_test::ReadText(file) : std::string("<none>");
}

/// @return A PUT answer that holds this name (JSON text of the name, or `null`).
ews::ApiAnswer Named(const std::string& jsonName) {
    return FakeSenders::Reply(200, "{\"device_id\":\"x\",\"device_name\":" + jsonName + ",\"created\":false}");
}

/// Lets the interval end, then says whether the log shows this count of dropped pipe messages.
bool DroppedInLog(Rig& rig, int count) {
    rig.Ticks(5);
    return Has(rig.Log(), " pipe_dropped " + std::to_string(count));
}

}  // namespace

EWS_TEST(RenameGoodNameWritesTheFileAndPutsAtOnce) {
    Rig rig("rename-good");
    rig.senders.putScript.push_back(Named("\"Lab PC\""));
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    CHECK(NameFile(rig) == "Lab PC\n");
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 1);
    CHECK(rig.senders.MethodOf(PutPath(rig), 0) == "PUT");
    CHECK(rig.senders.BodyOf(PutPath(rig), 0) == "{\"device_name\":\"Lab PC\",\"os\":\"windows\",\"device_type\":\"laptop\"}");
    CHECK(Has(rig.Status(), "\"last_ping_result\":\"never\",\"device_name\":\"Lab PC\",\"server_device_name\":\"Lab PC\",\"name_sync\":\"ok\"}\n"));
}

EWS_TEST(RenameEmptyValueDeletesTheFileAndSendsNull) {
    Rig rig("rename-empty");
    ews_test::WriteText(rig.data.path() / "Client" / "device-name.txt", "Old\n");
    rig.senders.putScript.push_back(Named("null"));
    StartOk(rig);
    rig.core.PipeMessage("EWS1 name=\n", true);
    CHECK(NameFile(rig) == "<none>");
    CHECK(rig.senders.BodyOf(PutPath(rig), 0) == "{\"device_name\":null,\"os\":\"windows\",\"device_type\":\"laptop\"}");
    std::string status = rig.Status();
    CHECK(!Has(status, ",\"device_name\":"));
    CHECK(Has(status, ",\"server_device_name\":null,\"name_sync\":\"ok\"}\n"));
}

EWS_TEST(RenameNameWithSpacesAndMultiByteLettersIsKept) {
    Rig rig("rename-utf8");
    StartOk(rig);
    rig.core.PipeMessage("EWS1 name=ICBDYWbDqSAg\n", true);  // two spaces, "Caf", an acute e, two spaces
    CHECK(NameFile(rig) == "Caf\xc3\xa9\n");
    CHECK(Has(rig.senders.BodyOf(PutPath(rig), 0), "{\"device_name\":\"Caf\xc3\xa9\","));
}

EWS_TEST(RenameBadMessagesAreDroppedCountedAndChangeNothing) {
    Rig rig("rename-bad");
    StartOk(rig);
    rig.core.PipeMessage("EWS1 name=TGFi*FBD\n", true);  // not base64
    rig.core.PipeMessage("EWS1 name=TGFiIFBD", true);    // no line feed
    rig.core.PipeMessage("EWS1 name=TGFiAVBD\n", true);  // control character
    rig.core.PipeMessage("EWS1 name=Qf8=\n", true);      // not UTF-8
    rig.core.PipeMessage("EWS1 name=wIA=\n", true);      // overlong UTF-8
    rig.core.PipeMessage("EWS1 name=IA==\n", true);      // only a space
    rig.core.PipeMessage("EWS1 name=YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYQ==\n", true);  // 61 characters
    rig.core.PipeMessage("EWS1 name=" + std::string(300, 'A') + "\n", true);  // over 256 bytes
    CHECK(NameFile(rig) == "<none>");
    CHECK(rig.senders.CallsTo(PutPath(rig)).empty());
    CHECK(DroppedInLog(rig, 8));
}

EWS_TEST(RenameSixtyCharactersPassAndSixtyOneDoNot) {
    Rig rig("rename-limit");
    StartOk(rig);
    std::string unit = "w6nDqcOp";  // three acute e letters
    std::string e60;
    for (int i = 0; i < 20; ++i) {
        e60 += unit;
    }
    std::string e61 = e60.substr(0, e60.size() - 0) + "w6k=";
    rig.core.PipeMessage("EWS1 name=" + e61 + "\n", true);
    CHECK(NameFile(rig) == "<none>");
    rig.core.PipeMessage("EWS1 name=" + e60 + "\n", true);
    CHECK(NameFile(rig).size() == 121);
}

EWS_TEST(RenameInactiveSessionIsDropped) {
    Rig rig("rename-inactive");
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, false);
    CHECK(NameFile(rig) == "<none>");
    CHECK(rig.senders.CallsTo(PutPath(rig)).empty());
}

EWS_TEST(RenameAtMostOnceInTenSeconds) {
    Rig rig("rename-rate");
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    rig.clock.Advance(9);
    rig.core.PipeMessage(kCafe, true);
    CHECK(NameFile(rig) == "Lab PC\n");
    rig.clock.Advance(1);
    rig.core.PipeMessage(kCafe, true);
    CHECK(NameFile(rig) == "Caf\xc3\xa9\n");
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 2);
    CHECK(DroppedInLog(rig, 1));
}

EWS_TEST(RenameServerKeepsAnotherNameAndStatusSaysSo) {
    Rig rig("rename-other");
    rig.senders.putScript.push_back(Named("\"Admin Name\""));
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    CHECK(Has(rig.Status(), ",\"device_name\":\"Lab PC\",\"server_device_name\":\"Admin Name\",\"name_sync\":\"ok\"}\n"));
}

EWS_TEST(RenameFieldsAreAbsentBeforeAnyRenameOrPut) {
    Rig rig("rename-absent");
    StartOk(rig);
    rig.Ticks(2);
    std::string status = rig.Status();
    CHECK(!Has(status, "device_name"));
    CHECK(!Has(status, "name_sync"));
}

EWS_TEST(RenameFailedPutStaysPendingAndIsRetriedOnThePingTick) {
    Rig rig("rename-retry");
    rig.senders.putScript.push_back(FakeSenders::Reply(500, "x"));
    rig.senders.putScript.push_back(FakeSenders::Reply(0, ""));
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    CHECK(Has(rig.Status(), ",\"device_name\":\"Lab PC\",\"name_sync\":\"pending\"}\n"));
    CHECK(Has(rig.data.Text("status\\name-sync.json"), "pending"));
    rig.Ticks(1);
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 2);
    CHECK(Has(rig.Status(), "\"name_sync\":\"pending\""));
    rig.Ticks(1);
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 3);
    CHECK(Has(rig.Status(), "\"name_sync\":\"ok\""));
    rig.Ticks(2);
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 3);
    CHECK(Has(rig.data.Text("status\\name-sync.json"), "ok"));
}

EWS_TEST(RenamePendingSurvivesARestart) {
    Rig rig("rename-restart");
    rig.senders.putScript.push_back(FakeSenders::Reply(503, "{}", 0));
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    rig.core.Stop();
    ews::ServiceCore second(rig.deps, ews::ServiceSettings{rig.data.wide(), 60});
    CHECK(second.Start() == ews::StartResult::Ok);
    CHECK(Has(rig.Status(), "\"name_sync\":\"pending\""));
    rig.clock.Advance(60);
    second.Tick();
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 2);
    CHECK(Has(rig.Status(), "\"name_sync\":\"ok\""));
    second.Stop();
}

EWS_TEST(RenameUnauthorisedPutFailsAndIsNotRetried) {
    Rig rig("rename-401");
    rig.senders.putScript.push_back(FakeSenders::Reply(401, "{}"));
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    CHECK(Has(rig.Status(), "\"name_sync\":\"failed\""));
    rig.Ticks(4);
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 1);
    CHECK(Has(rig.Status(), "\"name_sync\":\"failed\""));
}

EWS_TEST(RenameRejectedPutFailsAndIsNotRetried) {
    Rig rig("rename-422");
    rig.senders.putScript.push_back(FakeSenders::Reply(422, "{}"));
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    rig.Ticks(3);
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 1);
    CHECK(Has(rig.Status(), "\"name_sync\":\"failed\""));
}

EWS_TEST(RenameWaitsForRetryAfterBeforeTheNextPut) {
    Rig rig("rename-hold");
    rig.senders.putScript.push_back(FakeSenders::Reply(429, "{}", 180));
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    rig.Ticks(2);
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 1);
    rig.Ticks(3);
    CHECK(rig.senders.CallsTo(PutPath(rig)).size() == 2);
}

EWS_TEST(RenameRegistrationPutFillsTheServerName) {
    Rig rig("rename-register");
    rig.senders.pingDefault = FakeSenders::Reply(200, FakeSenders::PingJson(1, false, ""));
    rig.senders.putDefault = Named("\"Admin Name\"");
    StartOk(rig);
    rig.Ticks(1);
    std::string status = rig.Status();
    CHECK(Has(status, "\"server_device_name\":\"Admin Name\",\"name_sync\":\"ok\"}\n"));
    CHECK(!Has(status, ",\"device_name\":"));
}

EWS_TEST(RenameWithoutAServerKeepsTheNamePending) {
    Rig rig("rename-noserver");
    rig.WriteIni("server_enabled=0\ninterval_seconds=300\n");
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    CHECK(NameFile(rig) == "Lab PC\n");
    CHECK(rig.senders.calls.empty());
    CHECK(Has(rig.Status(), "\"device_name\":\"Lab PC\",\"name_sync\":\"pending\"}\n"));
}

EWS_TEST(RenameLogHoldsNoName) {
    Rig rig("rename-log");
    StartOk(rig);
    rig.core.PipeMessage(kLabPc, true);
    rig.Ticks(5);
    CHECK(!Has(rig.Log(), "Lab"));
    CHECK(!Has(rig.Log(), "TGFi"));
}
