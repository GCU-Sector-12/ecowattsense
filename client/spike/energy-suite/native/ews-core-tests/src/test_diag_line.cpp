// test_diag_line.cpp: tests of the line builders of the diagnostics file (ews/diag_line.h, SPEC 7a).
// Every input is fixed, so every expected line is written out in full. Nothing here touches a file or the clock.
#include <cmath>
#include <limits>
#include <string>

#include "ews/diag_line.h"
#include "test_common.h"

namespace {

constexpr std::int64_t kTime = 1794650700;  // 2026-11-14T10:05:00Z

/// @return The tick of the example in SPEC 7a.
ews::DiagTick FullTick() {
    ews::DiagTick tick;
    tick.unixSeconds = kTime;
    tick.seconds = 60;
    tick.onBattery = false;
    ews::DiagEmi emi;
    emi.name = "RAPL_Package0_PKG";
    emi.kind = "cpu_package";
    emi.ok = true;
    emi.raw = 123456789012ull;
    emi.uj = 5400000ull;
    tick.emi.push_back(emi);
    ews::DiagBattery battery;
    battery.ok = true;
    battery.remaining = 31200;
    battery.full = 36092;
    battery.design = 41000;
    battery.rate = -8200;
    battery.voltage = 11400;
    battery.discharging = true;
    tick.battery.push_back(battery);
    tick.cpuPct = 18.6;
    tick.display.on = true;
    tick.display.brightnessPct = 50;
    tick.display.from = "service";
    tick.session.loggedOn = true;
    tick.session.locked = false;
    tick.session.idleS = 12;
    tick.session.idleMsgAgeS = 30;
    tick.session.state = 2;
    tick.calc.scanLevel = "L2";
    tick.calc.level = "L2";
    tick.calc.model = true;
    tick.calc.measuredUj = 4100000;
    tick.calc.restW = 2.24;
    tick.calc.cpuTermW = 0.167;
    tick.calc.screenW = 1.40;
    tick.calc.deviceW = 7.91;
    tick.calc.batteryW = -8.2;
    tick.calc.energyUj = 474600000;
    tick.flags.push_back("battery_jump");
    return tick;
}

/// @return A tick with nothing known: only the values that are never unknown.
ews::DiagTick EmptyTick() {
    ews::DiagTick tick;
    tick.unixSeconds = kTime;
    tick.display.from = "default";
    tick.calc.scanLevel = "L4";
    return tick;
}

/// @return True when the text contains the part.
bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

EWS_TEST(DiagTickLineHasEveryField) {
    const std::string expected =
        R"({"t":"tick","ts":"2026-11-14T10:05:00Z","seconds":60,"power":{"on_battery":false},)"
        R"("emi":[{"name":"RAPL_Package0_PKG","kind":"cpu_package","ok":true,"raw":123456789012,"uj":5400000}],)"
        R"("battery":[{"ok":true,"unit":"mWh","remaining_mwh":31200,"full_mwh":36092,"design_mwh":41000,"voltage_mv":11400,"rate_mw":-8200,"online":false,"charging":false,"discharging":true}],)"
        R"("cpu":{"pct":18.6},"display":{"on":true,"brightness_pct":50,"brightness_from":"service"},)"
        R"("session":{"logged_on":true,"locked":false,"idle_s":12,"idle_msg_age_s":30,"state":2},)"
        R"("calc":{"scan_level":"L2","level":"L2","model":true,"measured_uj":4100000,"rest_w":2.240,"cpu_term_w":0.167,"screen_w":1.400,"device_w":7.910,"battery_w":-8.200,"energy_uj":474600000},)"
        R"("flags":["battery_jump"]})";
    CHECK(ews::BuildTickLine(FullTick()) == expected);
}

EWS_TEST(DiagTickLineLeavesOutUnknownValues) {
    const std::string expected =
        R"({"t":"tick","ts":"2026-11-14T10:05:00Z","power":{"on_battery":false},"emi":[],"battery":[],)"
        R"("display":{"on":true,"brightness_from":"default"},"session":{"logged_on":false,"locked":false},)"
        R"("calc":{"scan_level":"L4","model":false},"flags":[]})";
    CHECK(ews::BuildTickLine(EmptyTick()) == expected);
}

EWS_TEST(DiagTickLineStartsWithTypeAndTime) {
    CHECK(ews::BuildTickLine(EmptyTick()).rfind(R"({"t":"tick","ts":"2026-11-14T10:05:00Z")", 0) == 0);
    CHECK(ews::BuildReportLine(kTime, 1, "{}").rfind(R"({"t":"report","ts":"2026-11-14T10:05:00Z")", 0) == 0);
    ews::DiagSend send;
    CHECK(ews::BuildSendLine(kTime, send).rfind(R"({"t":"send","ts":"2026-11-14T10:05:00Z")", 0) == 0);
    CHECK(ews::BuildEventLine(kTime, "stop", {}).rfind(R"({"t":"event","ts":"2026-11-14T10:05:00Z")", 0) == 0);
}

EWS_TEST(DiagTickLineFailedChannelHasNoRawAndNoEnergy) {
    ews::DiagTick tick = EmptyTick();
    ews::DiagEmi bad;
    bad.name = "SYSTEM";
    bad.kind = "system";
    bad.ok = false;
    bad.raw = 5;  // a failed read has no value: the builder must not write it even when it is set
    bad.uj = 6;
    tick.emi.push_back(bad);
    CHECK(Has(ews::BuildTickLine(tick), R"("emi":[{"name":"SYSTEM","kind":"system","ok":false}])"));
}

EWS_TEST(DiagTickLineCounterThatWentBackKeepsRawWithoutEnergy) {
    ews::DiagTick tick = EmptyTick();
    ews::DiagEmi back;
    back.name = "RAPL_Package0_PKG";
    back.kind = "cpu_package";
    back.ok = true;
    back.raw = 77;
    tick.emi.push_back(back);
    CHECK(Has(ews::BuildTickLine(tick), R"("emi":[{"name":"RAPL_Package0_PKG","kind":"cpu_package","ok":true,"raw":77}])"));
}

EWS_TEST(DiagTickLineEscapesAChannelName) {
    ews::DiagTick tick = EmptyTick();
    ews::DiagEmi emi;
    emi.name = std::string("A\"B\\C") + '\x01' + "D";
    emi.kind = "other";
    tick.emi.push_back(emi);
    std::string line = ews::BuildTickLine(tick);
    CHECK(Has(line, R"("name":"A\"B\\C\u0001D")"));
    CHECK(line.find('\n') == std::string::npos);
}

EWS_TEST(DiagTickLineBatteryFailedAndRelative) {
    ews::DiagTick tick = EmptyTick();
    ews::DiagBattery failed;
    failed.ok = false;
    failed.remaining = 5;
    tick.battery.push_back(failed);
    ews::DiagBattery relative;
    relative.ok = true;
    relative.unitMwh = false;
    relative.remaining = 80;
    relative.full = 100;
    relative.design = 100;
    relative.rate = -10;
    relative.voltage = 12000;
    relative.online = true;
    tick.battery.push_back(relative);
    const std::string expected =
        R"("battery":[{"ok":false},{"ok":true,"unit":"relative","voltage_mv":12000,"online":true,"charging":false,"discharging":false}])";
    CHECK(Has(ews::BuildTickLine(tick), expected));
}

EWS_TEST(DiagTickLineLeavesOutNumbersThatAreNotFinite) {
    ews::DiagTick tick = EmptyTick();
    tick.cpuPct = std::numeric_limits<double>::quiet_NaN();
    tick.calc.deviceW = std::numeric_limits<double>::infinity();
    tick.calc.screenW = 0.5;
    std::string line = ews::BuildTickLine(tick);
    CHECK(!Has(line, R"("cpu")"));
    CHECK(!Has(line, "device_w"));
    CHECK(Has(line, R"("screen_w":0.500)"));
}

EWS_TEST(DiagTickLineNeverWritesMinusZero) {
    ews::DiagTick tick = EmptyTick();
    tick.calc.batteryW = -0.0001;
    tick.calc.deviceW = 0.0;
    std::string line = ews::BuildTickLine(tick);
    CHECK(Has(line, R"("battery_w":0.000)"));
    CHECK(Has(line, R"("device_w":0.000)"));
}

EWS_TEST(DiagTickLineWritesBaseWatts) {
    ews::DiagTick tick = EmptyTick();
    tick.calc.baseW = 3.48;
    CHECK(Has(ews::BuildTickLine(tick), R"("model":false,"base_w":3.480})"));
}

EWS_TEST(DiagReportLineHoldsTheReportExactly) {
    const std::string report = R"({"device_id":"a","timestamp":"2026-11-14T10:05:00Z","cpu_percent":20.0})";
    const std::string expected = R"({"t":"report","ts":"2026-11-14T10:05:00Z","queue":3,"report":)" + report + "}";
    CHECK(ews::BuildReportLine(kTime, 3, report) == expected);
}

EWS_TEST(DiagReportLineRefusesATextThatIsNotOneLine) {
    CHECK(ews::BuildReportLine(kTime, 1, "").empty());
    CHECK(ews::BuildReportLine(kTime, 1, "{\"a\":1}\n").empty());
    CHECK(ews::BuildReportLine(kTime, 1, "{\"a\":\r1}").empty());
}

EWS_TEST(DiagSendLineWithAnAnswer) {
    ews::DiagSend send;
    send.trigger = "tick";
    send.reports = 3;
    send.result = "ok";
    send.http = 201;
    send.created = 3;
    send.duplicate = 0;
    send.rejected = 0;
    send.queue = 0;
    const std::string expected =
        R"({"t":"send","ts":"2026-11-14T10:05:00Z","trigger":"tick","reports":3,"result":"ok","http":201,"created":3,"duplicate":0,"rejected":0,"queue":0})";
    CHECK(ews::BuildSendLine(kTime, send) == expected);
}

EWS_TEST(DiagSendLineWithoutAnAnswerLeavesTheAnswerOut) {
    ews::DiagSend send;
    send.trigger = "network";
    send.reports = 1;
    send.result = "network";
    send.queue = 1;
    const std::string expected = R"({"t":"send","ts":"2026-11-14T10:05:00Z","trigger":"network","reports":1,"result":"network","queue":1})";
    CHECK(ews::BuildSendLine(kTime, send) == expected);
}

EWS_TEST(DiagEventLineFieldsAndUnknownValues) {
    CHECK(ews::BuildEventLine(kTime, "power_source", {ews::FieldBool("on_battery", true)}) ==
          R"({"t":"event","ts":"2026-11-14T10:05:00Z","what":"power_source","on_battery":true})");
    CHECK(ews::BuildEventLine(kTime, "stop", {}) == R"({"t":"event","ts":"2026-11-14T10:05:00Z","what":"stop"})");
    std::vector<ews::DiagField> fields = {ews::FieldMaybeInt("settings_version", std::nullopt), ews::FieldInt("report_interval_seconds", 300),
                                          ews::FieldMaybeInt("ping_interval_seconds", 60), ews::FieldWord("result", "ok")};
    CHECK(ews::BuildEventLine(kTime, "x", fields) ==
          R"({"t":"event","ts":"2026-11-14T10:05:00Z","what":"x","report_interval_seconds":300,"ping_interval_seconds":60,"result":"ok"})");
}

EWS_TEST(DiagEventLineLeavesOutAnEmptyWord) {
    CHECK(ews::BuildEventLine(kTime, "level_change", {ews::FieldWord("from", ""), ews::FieldWord("to", "L2")}) ==
          R"({"t":"event","ts":"2026-11-14T10:05:00Z","what":"level_change","to":"L2"})");
}

EWS_TEST(DiagEventLineEscapesAWord) {
    CHECK(ews::BuildEventLine(kTime, "x", {ews::FieldWord("w", "a\"b")}) == R"({"t":"event","ts":"2026-11-14T10:05:00Z","what":"x","w":"a\"b"})");
}

EWS_TEST(DiagLineLimitIs16KbWithTheLineFeed) {
    CHECK(ews::FitsDiagLine(std::string(16383, 'a')));
    CHECK(!ews::FitsDiagLine(std::string(16384, 'a')));
    CHECK(ews::FitsDiagLine(std::string()));
}
