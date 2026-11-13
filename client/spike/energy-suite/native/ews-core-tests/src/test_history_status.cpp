// test_history_status.cpp: tests of the history row (ews/history_format.h, SPEC 7) and status.json (ews/status_format.h, SPEC 5.7).
#include <string>

#include "ews/history_format.h"
#include "ews/status_format.h"
#include "test_calc_common.h"
#include "test_common.h"

using ews::BuildStatusJson;
using ews::FormatHistoryRow;
using ews::HistoryRow;
using ews::SendResult;
using ews::StatusFields;

EWS_TEST(HistoryHeaderIsExact) {
    CHECK(std::string(ews::kHistoryHeader) ==
          "v1,ts_utc,seconds,on_battery,state,idle_s,cpu_pct,device_w,measured_w,battery_w,level");
}

EWS_TEST(HistoryRowAllValues) {
    HistoryRow row;
    row.timestampUnix = ews_test::kTestTime;
    row.seconds = 60;
    row.onBattery = true;
    row.state = 2;
    row.idleSeconds = 45;
    row.cpuPct = 12.34;
    row.deviceW = 7.5;
    row.measuredW = 4.1234;
    row.batteryW = -9.87654;
    row.level = ews::MeasureLevel::L2;
    CHECK(FormatHistoryRow(row) == "v1,2026-11-14T10:05:00Z,60,1,2,45,12.3,7.500,4.123,-9.877,L2");
}

EWS_TEST(HistoryRowUnknownValuesAreEmpty) {
    HistoryRow row;
    row.timestampUnix = ews_test::kTestTime;
    row.seconds = 60;
    row.onBattery = false;
    CHECK(FormatHistoryRow(row) == "v1,2026-11-14T10:05:00Z,60,0,,,,,,,");
}

EWS_TEST(HistoryRowNonFiniteNumberIsEmpty) {
    HistoryRow row;
    row.timestampUnix = ews_test::kTestTime;
    row.seconds = 60;
    row.deviceW = std::nan("");
    row.level = ews::MeasureLevel::L1;
    CHECK(FormatHistoryRow(row) == "v1,2026-11-14T10:05:00Z,60,0,,,,,,,L1");
}

EWS_TEST(SendResultWords) {
    CHECK(std::string(ews::SendResultWord(SendResult::Ok)) == "ok");
    CHECK(std::string(ews::SendResultWord(SendResult::Disabled)) == "disabled");
    CHECK(std::string(ews::SendResultWord(SendResult::Unauthorised)) == "unauthorised");
    CHECK(std::string(ews::SendResultWord(SendResult::Rejected)) == "rejected");
    CHECK(std::string(ews::SendResultWord(SendResult::ServerError)) == "server_error");
    CHECK(std::string(ews::SendResultWord(SendResult::Network)) == "network");
    CHECK(std::string(ews::SendResultWord(SendResult::Never)) == "never");
}

EWS_TEST(StatusJsonAllValues) {
    StatusFields s;
    s.serviceVersion = "0.1.0";
    s.level = "L2";
    s.modelGeneratedUtc = "2026-11-12T17:08:31Z";
    s.lastReportUnix = ews_test::kTestTime;
    s.lastSendUnix = ews_test::kTestTime + 1;
    s.lastSendResult = SendResult::Ok;
    s.queued = 0;
    CHECK(BuildStatusJson(s) ==
          "{\"schema\":1,\"service_version\":\"0.1.0\",\"level\":\"L2\",\"model_generated_utc\":\"2026-11-12T17:08:31Z\","
          "\"last_report_utc\":\"2026-11-14T10:05:00Z\",\"last_send_utc\":\"2026-11-14T10:05:01Z\","
          "\"last_send_result\":\"ok\",\"queued\":0}\n");
}

EWS_TEST(StatusJsonUnknownValuesAreLeftOut) {
    StatusFields s;
    s.serviceVersion = "0.1.0";
    s.lastSendResult = SendResult::Never;
    s.queued = 3;
    CHECK(BuildStatusJson(s) == "{\"schema\":1,\"service_version\":\"0.1.0\",\"last_send_result\":\"never\",\"queued\":3}\n");
}
