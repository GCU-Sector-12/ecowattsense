// test_scan_output.cpp: tests of the capabilities.json writer and the scan.log writer (exact text, escaping, masking).
#include <string>

#include "ews/scan_output.h"
#include "fake_sources.h"
#include "test_common.h"

using ews::Tri;

namespace {

/// Builds one small result with known text output.
ews::ScanResult SmallResult() {
    ews::ScanResult r;
    r.scannedUtc = "2026-11-14T10:00:00Z";
    r.machine.smbios.maker = "HP";
    r.machine.smbios.chassisType = 10;
    r.machine.logicalProcessors = 8;

    r.emi.common = {Tri::True, Tri::True, Tri::True, 0, 4.1};
    ews::EmiChannelResult channel;
    channel.name = "RAPL_Package0_PKG";
    channel.kind = ews::EmiKind::CpuPackage;
    channel.live = Tri::True;
    channel.sampleWatts = 4.1;
    r.emi.channels.push_back(channel);

    r.battery.common = {Tri::True, Tri::True, Tri::Unknown, 0, std::nullopt};
    r.battery.count = 1;
    r.battery.unit = ews::BatteryUnit::MilliwattHours;
    r.battery.rateKnown = false;
    r.battery.fullMwh = 36092;

    r.powerMeter.common = {Tri::True, Tri::True, Tri::False, 0, 0.0};
    r.powerMeter.instances = 1;

    r.display.common = {Tri::True, Tri::False, Tri::Unknown, 5, std::nullopt};

    r.gpu.common = {Tri::True, Tri::True, Tri::Unknown, 0, std::nullopt};
    ews::GpuAdapter adapter;
    adapter.description = "Test";
    adapter.vendorId = 0x8086;
    adapter.dedicatedMemoryMegabytes = 128;
    r.gpu.adapters.push_back(adapter);
    r.gpu.nvml = ews::NvmlState::Unavailable;

    r.intelDtt.common = {Tri::False, Tri::True, Tri::Unknown, 0, std::nullopt};

    r.level = ews::MeasurementLevel::L2;
    r.calibration = ews::Calibration::Needed;
    r.deviceType = ews::DeviceType::Laptop;
    return r;
}

/// @return True when `text` holds `part`.
bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

EWS_TEST(JsonExactTextOfASmallResult) {
    const std::string expected =
        "{\n"
        "  \"schema\": 1,\n"
        "  \"scanned_utc\": \"2026-11-14T10:00:00Z\",\n"
        "  \"context\": \"user\",\n"
        "  \"machine\": { \"maker\": \"HP\", \"chassis\": \"laptop\", \"logical_processors\": 8 },\n"
        "  \"sources\": [\n"
        "    { \"id\": \"emi\", \"present\": true, \"readable\": true, \"live\": true, \"error\": 0, "
        "\"channels\": [ { \"device\": 0, \"index\": 0, \"name\": \"RAPL_Package0_PKG\", \"kind\": \"cpu_package\", "
        "\"live\": true, \"sample_w\": 4.1 } ] },\n"
        "    { \"id\": \"battery\", \"present\": true, \"readable\": true, \"live\": null, \"error\": 0, "
        "\"count\": 1, \"unit\": \"mWh\", \"rate_known\": false, \"full_mwh\": 36092 },\n"
        "    { \"id\": \"acpi_power_meter\", \"present\": true, \"readable\": true, \"live\": false, \"error\": 0, "
        "\"instances\": 1, \"sample\": 0.0 },\n"
        "    { \"id\": \"display\", \"present\": true, \"readable\": false, \"live\": null, \"error\": 5 },\n"
        "    { \"id\": \"gpu\", \"present\": true, \"readable\": true, \"live\": null, \"error\": 0, "
        "\"adapters\": [ { \"name\": \"Test\", \"vendor_id\": 32902, \"memory_mb\": 128, \"software\": false } ], "
        "\"nvml\": \"unavailable\" },\n"
        "    { \"id\": \"intel_dtt\", \"present\": false, \"readable\": true, \"live\": null, \"error\": 0 }\n"
        "  ],\n"
        "  \"level\": \"L2\",\n"
        "  \"calibration\": \"needed\",\n"
        "  \"device_type\": \"laptop\"\n"
        "}\n";
    CHECK(ews::WriteCapabilitiesJson(SmallResult()) == expected);
}

EWS_TEST(JsonEscapesQuoteBackslashTabAndKeepsUtf8) {
    auto r = SmallResult();
    r.machine.smbios.model = std::string("A\"B\\C\tD \xC3\xA9");
    auto text = ews::WriteCapabilitiesJson(r);
    CHECK(Has(text, "\"model\": \"A\\\"B\\\\C\\u0009D \xC3\xA9\""));
}

EWS_TEST(JsonEscapesOtherControlCharactersAndRejectsBadUtf8) {
    auto r = SmallResult();
    r.machine.smbios.model = std::string("a\nb\x01" "c\xFF" "d", 7);
    auto text = ews::WriteCapabilitiesJson(r);
    CHECK(Has(text, "\"model\": \"a\\u000ab\\u0001c?d\""));
}

EWS_TEST(JsonUnknownLiveIsNullAndUnknownFieldsAreAbsent) {
    ews::ScanResult r;  // everything unknown
    auto text = ews::WriteCapabilitiesJson(r);
    CHECK(Has(text, "\"live\": null"));
    CHECK(!Has(text, "\"present\""));
    CHECK(!Has(text, "\"readable\""));
    CHECK(!Has(text, "\"maker\""));
    CHECK(!Has(text, "\"count\""));
    CHECK(!Has(text, "\"sample\""));
    CHECK(Has(text, "\"machine\": { }"));
}

EWS_TEST(JsonSourcesAreInTheFixedOrder) {
    auto text = ews::WriteCapabilitiesJson(SmallResult());
    auto emi = text.find("\"id\": \"emi\"");
    auto battery = text.find("\"id\": \"battery\"");
    auto meter = text.find("\"id\": \"acpi_power_meter\"");
    auto display = text.find("\"id\": \"display\"");
    auto gpu = text.find("\"id\": \"gpu\"");
    auto dtt = text.find("\"id\": \"intel_dtt\"");
    CHECK(emi < battery);
    CHECK(battery < meter);
    CHECK(meter < display);
    CHECK(display < gpu);
    CHECK(gpu < dtt);
}

EWS_TEST(JsonNumbersUseADotAndThreeDecimalsAtMost) {
    auto r = SmallResult();
    r.emi.channels[0].sampleWatts = 12.0;
    r.powerMeter.common.sample = 0.1234567;
    auto text = ews::WriteCapabilitiesJson(r);
    CHECK(Has(text, "\"sample_w\": 12.0"));
    CHECK(Has(text, "\"sample\": 0.123"));
}

EWS_TEST(JsonNeverHoldsSerialUuidDevicePathOrPath) {
    ews_test::Scenario s;
    auto result = ews::RunScan(s.Sources(), ews::ScanContext::User, "2026-11-14T10:00:00Z");
    auto text = ews::WriteCapabilitiesJson(result);
    CHECK(!Has(text, "serial"));
    CHECK(!Has(text, "uuid"));
    CHECK(!Has(text, "\\\\?\\"));
    CHECK(!Has(text, "path"));
    CHECK(Has(text, "\"level\": \"L2\""));
}

EWS_TEST(LogHasOneLinePerStepAndTheDecisions) {
    auto log = ews::WriteScanLog(SmallResult());
    CHECK(Has(log, "emi step=present result=yes error=0 sample=-\n"));
    CHECK(Has(log, "emi step=live result=yes error=0 sample=-\n"));
    CHECK(Has(log, "emi step=sample result=ok error=0 sample=4.1\n"));
    CHECK(Has(log, "display step=readable result=no error=5 sample=-\n"));
    CHECK(Has(log, "display step=sample result=unknown error=5 sample=-\n"));
    CHECK(Has(log, "battery step=live result=unknown error=0 sample=-\n"));
    CHECK(Has(log, "level L2\n"));
    CHECK(Has(log, "calibration needed\n"));
    CHECK(Has(log, "device_type laptop\n"));
    CHECK(Has(log, "intel_dtt step=present result=no error=0 sample=-\n"));
}

EWS_TEST(FileNamesFollowTheContext) {
    CHECK(std::wstring(ews::CapabilitiesFileName(ews::ScanContext::User)) == L"capabilities.json");
    CHECK(std::wstring(ews::CapabilitiesFileName(ews::ScanContext::Admin)) == L"capabilities.json");
    CHECK(std::wstring(ews::CapabilitiesFileName(ews::ScanContext::Service)) == L"capabilities.service.json");
    CHECK(std::wstring(ews::ScanLogFileName(ews::ScanContext::User)) == L"scan.log");
    CHECK(std::wstring(ews::ScanLogFileName(ews::ScanContext::Service)) == L"scan.service.log");
}
