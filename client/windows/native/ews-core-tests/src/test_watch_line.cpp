// test_watch_line.cpp: tests of the text of the watch mode (SPEC 1.6) with fixed inputs.
#include <string>

#include "ews/watch_line.h"
#include "test_common.h"

namespace {

/// @return True when `text` holds `part`.
bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

/// Builds the header of the example of the spec.
ews::WatchHeader ExampleHeader() {
    ews::WatchHeader header;
    header.periodMs = 2000;
    header.emi.push_back({"RAPL_Package0_PKG", "cpu_package", 0, 0});
    ews::WatchNvmlEntry nvml;
    nvml.name = "GPU 1";
    nvml.adapter = 1;
    nvml.power = true;
    nvml.energy = true;
    header.nvml.push_back(nvml);
    header.gpuUse.push_back({0, std::string("power_saving"), 4098});
    header.machineJson = "{ \"maker\": \"HP\" }";
    header.batteryJson = "{ \"id\": \"battery\" }";
    header.displayJson = "{ \"id\": \"display\" }";
    return header;
}

}  // namespace

EWS_TEST(WatchHeaderExactText) {
    const std::string expected =
        "{\"watch\":1,\"period_ms\":2000,"
        "\"emi\":[{\"name\":\"RAPL_Package0_PKG\",\"kind\":\"cpu_package\",\"device\":0,\"index\":0}],"
        "\"nvml\":[{\"name\":\"GPU 1\",\"adapter\":1,\"power\":true,\"energy\":true}],"
        "\"gpu_use\":[{\"adapter\":0,\"role\":\"power_saving\",\"vendor_id\":4098}],"
        "\"machine\":{ \"maker\": \"HP\" },\"battery\":{ \"id\": \"battery\" },\"display\":{ \"id\": \"display\" }}\n";
    CHECK(ews::BuildWatchHeaderLine(ExampleHeader()) == expected);
}

EWS_TEST(WatchHeaderLeavesOutAnUnknownAdapterAndRole) {
    auto header = ExampleHeader();
    header.nvml[0].adapter.reset();
    header.nvml[0].energy = false;
    header.gpuUse[0].roleWord.reset();
    auto text = ews::BuildWatchHeaderLine(header);
    CHECK(Has(text, "\"nvml\":[{\"name\":\"GPU 1\",\"power\":true,\"energy\":false}]"));
    CHECK(Has(text, "\"gpu_use\":[{\"adapter\":0,\"vendor_id\":4098}]"));
}

EWS_TEST(WatchHeaderWithEmptyListsIsStillOneLine) {
    ews::WatchHeader header;
    header.periodMs = 500;
    header.machineJson = "{ }";
    header.batteryJson = "{ }";
    header.displayJson = "{ }";
    auto text = ews::BuildWatchHeaderLine(header);
    CHECK(text == "{\"watch\":1,\"period_ms\":500,\"emi\":[],\"nvml\":[],\"gpu_use\":[],\"machine\":{ },\"battery\":{ },\"display\":{ }}\n");
    CHECK(text.find('\n') == text.size() - 1);
}

EWS_TEST(WatchHeaderEscapesNames) {
    auto header = ExampleHeader();
    header.nvml[0].name = "a\"b\\c\xFF";
    CHECK(Has(ews::BuildWatchHeaderLine(header), "\"name\":\"a\\\"b\\\\c?\""));
}

EWS_TEST(WatchSampleLineExactText) {
    ews::WatchOutput out;
    out.tMs = 4000;
    out.emiUj = {123456, std::nullopt};
    out.nvmlMw = {15300};
    out.nvmlMj = {98765};
    out.gpuPct = {12.5, 0.0};
    CHECK(ews::BuildWatchSampleLine(out) ==
          "{\"t_ms\":4000,\"emi_uj\":[123456,null],\"nvml_mw\":[15300],\"nvml_mj\":[98765],\"gpu_pct\":[12.5,0.0]}\n");
}

EWS_TEST(WatchSampleLineNullsAndEmptyLists) {
    ews::WatchOutput out;
    out.tMs = 0;
    out.nvmlMw = {std::nullopt};
    out.nvmlMj = {std::nullopt};
    out.gpuPct = {std::nullopt};
    CHECK(ews::BuildWatchSampleLine(out) == "{\"t_ms\":0,\"emi_uj\":[],\"nvml_mw\":[null],\"nvml_mj\":[null],\"gpu_pct\":[null]}\n");
}

EWS_TEST(WatchSampleLineGpuPercentHasOneDecimalAndADot) {
    ews::WatchOutput out;
    out.gpuPct = {33.349, 99.95, 100.0, 7.0};
    auto text = ews::BuildWatchSampleLine(out);
    CHECK(Has(text, "\"gpu_pct\":[33.3,100.0,100.0,7.0]"));
}
