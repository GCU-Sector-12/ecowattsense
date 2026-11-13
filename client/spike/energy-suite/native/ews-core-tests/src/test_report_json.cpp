// test_report_json.cpp: tests of the report JSON builder (ews/report_json.h, SPEC 5.4) and the UTC time text.
#include <string>

#include "ews/json_text.h"
#include "ews/report_json.h"
#include "test_calc_common.h"
#include "test_common.h"

using ews::BuildReportJson;
using ews::ReportFields;

namespace {

/// @return A report with every field known.
ReportFields FullReport() {
    ReportFields f;
    f.deviceId = "123e4567-e89b-12d3-a456-426614174000";
    f.timestampUnix = ews_test::kTestTime;
    f.intervalSeconds = 300;
    f.idleSeconds = 45;
    f.state = 2;
    f.cpuPercent = 12.34;
    f.screenLocked = false;
    f.onBattery = false;
    f.deviceType = "laptop";
    f.energyUj = 536000000;
    f.batteryEnergyUj = 36000000;
    f.deviceEnergyUj = 500000000;
    return f;
}

}  // namespace

EWS_TEST(ReportJsonAllFields) {
    auto r = BuildReportJson(FullReport());
    CHECK(r.ok());
    CHECK(r.value ==
          "{\"device_id\":\"123e4567-e89b-12d3-a456-426614174000\",\"timestamp\":\"2026-11-14T10:05:00Z\","
          "\"interval_seconds\":300,\"idle_seconds\":45,\"state\":2,\"cpu_percent\":12.3,\"screen_locked\":false,"
          "\"on_battery\":false,\"os\":\"windows\",\"device_type\":\"laptop\",\"energy_uj\":536000000,"
          "\"battery_energy_uj\":36000000,\"device_energy_uj\":500000000}");
}

EWS_TEST(ReportJsonWithoutOptionalFields) {
    ReportFields f = FullReport();
    f.cpuPercent.reset();
    f.energyUj.reset();
    f.batteryEnergyUj.reset();
    f.deviceEnergyUj.reset();
    f.screenLocked = true;
    f.onBattery = true;
    f.state = 0;
    auto r = BuildReportJson(f);
    CHECK(r.ok());
    CHECK(r.value ==
          "{\"device_id\":\"123e4567-e89b-12d3-a456-426614174000\",\"timestamp\":\"2026-11-14T10:05:00Z\","
          "\"interval_seconds\":300,\"idle_seconds\":45,\"state\":0,\"screen_locked\":true,"
          "\"on_battery\":true,\"os\":\"windows\",\"device_type\":\"laptop\"}");
}

EWS_TEST(ReportJsonNegativeBatteryEnergy) {
    ReportFields f = FullReport();
    f.batteryEnergyUj = -72000000;
    auto r = BuildReportJson(f);
    CHECK(r.ok());
    CHECK(r.value.find("\"battery_energy_uj\":-72000000") != std::string::npos);
}

EWS_TEST(ReportJsonBadDeviceType) {
    ReportFields f = FullReport();
    const char* bad[] = {"", "Laptop", "1abc", "a b", "a-b", "lap\"top", "_x", "laptop\n"};
    for (const char* value : bad) {
        f.deviceType = value;
        CHECK(!BuildReportJson(f).ok());
    }
    f.deviceType = std::string(41, 'a');
    CHECK(!BuildReportJson(f).ok());
    f.deviceType = std::string(40, 'a');
    CHECK(BuildReportJson(f).ok());
    f.deviceType = "my_device_2";
    CHECK(BuildReportJson(f).ok());
}

EWS_TEST(ReportJsonBadNumbers) {
    ReportFields f = FullReport();
    f.intervalSeconds = 0;
    CHECK(!BuildReportJson(f).ok());
    f = FullReport();
    f.idleSeconds = -1;
    CHECK(!BuildReportJson(f).ok());
    f = FullReport();
    f.state = 3;
    CHECK(!BuildReportJson(f).ok());
}

EWS_TEST(ReportJsonCpuPercentRounding) {
    ReportFields f = FullReport();
    f.cpuPercent = 100.0;
    CHECK(BuildReportJson(f).value.find("\"cpu_percent\":100.0,") != std::string::npos);
    f.cpuPercent = 0.04;
    CHECK(BuildReportJson(f).value.find("\"cpu_percent\":0.0,") != std::string::npos);
    f.cpuPercent = 0.05;
    CHECK(BuildReportJson(f).value.find("\"cpu_percent\":0.1,") != std::string::npos);
    f.cpuPercent = -0.0;
    CHECK(BuildReportJson(f).value.find("\"cpu_percent\":0.0,") != std::string::npos);
}

EWS_TEST(ReportJsonCpuPercentOutOfRangeIsClampedOrLeftOut) {
    ReportFields f = FullReport();
    f.cpuPercent = 140.0;
    CHECK(BuildReportJson(f).value.find("\"cpu_percent\":100.0,") != std::string::npos);
    f.cpuPercent = std::nan("");
    CHECK(BuildReportJson(f).value.find("cpu_percent") == std::string::npos);
}

EWS_TEST(UtcTimeText) {
    CHECK(ews::FormatUtcTime(0) == "1970-01-01T00:00:00Z");
    CHECK(ews::FormatUtcTime(ews_test::kTestTime) == "2026-11-14T10:05:00Z");
    CHECK(ews::FormatUtcTime(951782400) == "2000-02-29T00:00:00Z");
    CHECK(ews::FormatUtcTime(-5) == "1970-01-01T00:00:00Z");
}

EWS_TEST(FixedDecimalsText) {
    CHECK(ews::FormatFixed(4.1, 3) == "4.100");
    CHECK(ews::FormatFixed(-9.87654, 3) == "-9.877");
    CHECK(ews::FormatFixed(12.34, 1) == "12.3");
}
