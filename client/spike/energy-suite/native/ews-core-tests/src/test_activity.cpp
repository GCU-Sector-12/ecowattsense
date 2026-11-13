// test_activity.cpp: tests of the pipe rules (ews/activity.h, SPEC section 6): idle seconds, state, message parser.
#include <string>

#include "ews/activity.h"
#include "test_common.h"

using ews::ComputeIdleSeconds;
using ews::ComputeState;
using ews::ParsePipeMessage;
using ews::SessionActivity;

namespace {

/// @return A message that arrived at 1000 s with an idle value of 30 s.
SessionActivity Last() {
    SessionActivity a;
    a.hasMessage = true;
    a.idleSeconds = 30;
    a.receivedUnix = 1000;
    return a;
}

}  // namespace

EWS_TEST(IdleFromFreshMessage) {
    CHECK(ComputeIdleSeconds(Last(), 1050, 0) == 80);
    CHECK(ComputeIdleSeconds(Last(), 1000, 0) == 30);
}

EWS_TEST(IdleMessageOf119SecondsIsFresh) {
    CHECK(ComputeIdleSeconds(Last(), 1119, 0) == 149);
}

EWS_TEST(IdleMessageOf120SecondsIsStale) {
    // "younger than 120 seconds" is fresh, so 120 is stale: the age counts alone (120), not 30 + 120.
    CHECK(ComputeIdleSeconds(Last(), 1120, 0) == 120);
}

EWS_TEST(IdleFromStaleMessage) {
    CHECK(ComputeIdleSeconds(Last(), 1500, 0) == 500);
}

EWS_TEST(IdleWithoutMessageCountsFromServiceStart) {
    SessionActivity none;
    CHECK(ComputeIdleSeconds(none, 1000, 900) == 100);
}

EWS_TEST(IdleClockWentBack) {
    CHECK(ComputeIdleSeconds(Last(), 900, 0) == 0);
    SessionActivity none;
    CHECK(ComputeIdleSeconds(none, 800, 900) == 0);
}

EWS_TEST(IdleIsLimitedTo32BitRange) {
    SessionActivity none;
    CHECK(ComputeIdleSeconds(none, 9000000000LL, 0) == 2147483647LL);
}

EWS_TEST(StateAtTheThresholdBorders) {
    CHECK(ComputeState(0, 600, 3600, false) == 2);
    CHECK(ComputeState(599, 600, 3600, false) == 2);
    CHECK(ComputeState(600, 600, 3600, false) == 1);
    CHECK(ComputeState(3599, 600, 3600, false) == 1);
    CHECK(ComputeState(3600, 600, 3600, false) == 0);
}

EWS_TEST(StateIsZeroWhenNobodyLoggedOn) {
    CHECK(ComputeState(0, 600, 3600, true) == 0);
}

EWS_TEST(PipeMessageGood) {
    auto r = ParsePipeMessage("EWS1 idle=123 bright=80\n");
    CHECK(r.ok());
    CHECK(r.value.idleSeconds == 123);
    CHECK(r.value.brightnessPct.has_value() && *r.value.brightnessPct == 80);
}

EWS_TEST(PipeMessageBrightnessDash) {
    auto r = ParsePipeMessage("EWS1 idle=0 bright=-\n");
    CHECK(r.ok());
    CHECK(r.value.idleSeconds == 0);
    CHECK(!r.value.brightnessPct.has_value());
}

EWS_TEST(PipeMessageLimits) {
    CHECK(ParsePipeMessage("EWS1 idle=2147483647 bright=100\n").ok());
    CHECK(ParsePipeMessage("EWS1 idle=0 bright=0\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=2147483648 bright=5\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=1 bright=101\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=-1 bright=5\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=99999999999999999999 bright=5\n").ok());
}

EWS_TEST(PipeMessageTooLong) {
    std::string longMessage = "EWS1 idle=1 bright=-" + std::string(108, ' ') + "\n";
    CHECK(longMessage.size() == 129);
    CHECK(!ParsePipeMessage(longMessage).ok());
}

EWS_TEST(PipeMessageNonAscii) {
    CHECK(!ParsePipeMessage("EWS1 idle=1 bright=\xC3\xA9\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=1 bright=5\x80\n").ok());
}

EWS_TEST(PipeMessageWrongShape) {
    CHECK(!ParsePipeMessage("EWS1 idle=1 bright=5 x=1\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=1 bright=5").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=1 bright=5\n\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=1 bright=5\r\n").ok());
    CHECK(!ParsePipeMessage("EWS2 idle=1 bright=5\n").ok());
    CHECK(!ParsePipeMessage("EWS1 bright=5 idle=1\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle= bright=5\n").ok());
    CHECK(!ParsePipeMessage("EWS1  idle=1 bright=5\n").ok());
    CHECK(!ParsePipeMessage("EWS1 idle=01 bright=5\n").ok());
    CHECK(!ParsePipeMessage("").ok());
    CHECK(!ParsePipeMessage("\n").ok());
}
