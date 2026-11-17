// test_model.cpp: tests of the calibration model parser (ews/model.h, SPEC section 8).
#include <string>

#include "ews/model.h"
#include "test_calc_common.h"
#include "test_common.h"

using ews::ParseModelJson;

namespace {

/// Replaces the first occurrence of `from` in the model text by `to`. The text must contain `from`.
std::string Changed(const std::string& from, const std::string& to) {
    std::string text = ews_test::SpecModelText();
    std::size_t at = text.find(from);
    CHECK(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

}  // namespace

EWS_TEST(ModelSpecExampleParses) {
    auto r = ParseModelJson(ews_test::SpecModelText());
    CHECK(r.ok());
    CHECK(r.value.generatedUtc == "2026-11-12T17:08:31Z");
    CHECK(r.value.measuresCpuPackage && r.value.measuresDram);
    CHECK(r.value.baseW == 3.48);
    CHECK(r.value.restW == 2.24);
    CHECK(r.value.restWPerCpuPct == 0.009);
    CHECK(r.value.cpuWPerPct == 0.075);
    CHECK(r.value.screenWAt50 == 1.40);
    CHECK(r.value.screenWPerPct == 0.0104);
    CHECK(r.value.graphicsW == 2.44);
}

EWS_TEST(ModelRejectsBadSchema) {
    CHECK(!ParseModelJson(Changed("\"schema\": 2", "\"schema\": 1")).ok());
    CHECK(!ParseModelJson(Changed("\"schema\": 2", "\"schema\": \"2\"")).ok());
    CHECK(!ParseModelJson(Changed("\"schema\": 2,", "")).ok());
}

EWS_TEST(ModelRejectsMissingNumber) {
    CHECK(!ParseModelJson(Changed("\"rest_w\": 2.24,", "")).ok());
}

EWS_TEST(ModelRejectsOutOfRange) {
    CHECK(!ParseModelJson(Changed("\"base_w\": 3.48", "\"base_w\": 501")).ok());
    CHECK(ParseModelJson(Changed("\"base_w\": 3.48", "\"base_w\": 500")).ok());
    CHECK(!ParseModelJson(Changed("\"cpu_w_per_pct\": 0.075", "\"cpu_w_per_pct\": 5.1")).ok());
    CHECK(ParseModelJson(Changed("\"cpu_w_per_pct\": 0.075", "\"cpu_w_per_pct\": 5")).ok());
    CHECK(!ParseModelJson(Changed("\"graphics_w\": 2.44", "\"graphics_w\": -0.1")).ok());
}

EWS_TEST(ModelRejectsNumberAsString) {
    CHECK(!ParseModelJson(Changed("\"base_w\": 3.48", "\"base_w\": \"3.48\"")).ok());
}

EWS_TEST(ModelRejectsNumberThatIsNotFinite) {
    CHECK(!ParseModelJson(Changed("\"base_w\": 3.48", "\"base_w\": NaN")).ok());
    CHECK(!ParseModelJson(Changed("\"base_w\": 3.48", "\"base_w\": Infinity")).ok());
    CHECK(!ParseModelJson(Changed("\"base_w\": 3.48", "\"base_w\": 1e999")).ok());
}

EWS_TEST(ModelMeasuredParts) {
    CHECK(!ParseModelJson(Changed("[\"cpu_package\", \"dram\"]", "[\"cpu_package\", \"gpu\"]")).ok());
    CHECK(!ParseModelJson(Changed("[\"cpu_package\", \"dram\"]", "[1]")).ok());
    CHECK(!ParseModelJson(Changed("\"measured_parts\": [\"cpu_package\", \"dram\"],", "")).ok());
    auto empty = ParseModelJson(Changed("[\"cpu_package\", \"dram\"]", "[]"));
    CHECK(empty.ok());
    CHECK(!empty.value.measuresCpuPackage && !empty.value.measuresDram);
    auto one = ParseModelJson(Changed("[\"cpu_package\", \"dram\"]", "[\"dram\"]"));
    CHECK(one.ok());
    CHECK(!one.value.measuresCpuPackage && one.value.measuresDram);
}

EWS_TEST(ModelGeneratedUtcShape) {
    CHECK(!ParseModelJson(Changed("2026-11-12T17:08:31Z", "2026-11-12T17:08:31")).ok());
    CHECK(!ParseModelJson(Changed("2026-11-12T17:08:31Z", "2026-11-12 17:08:31Z")).ok());
    CHECK(!ParseModelJson(Changed("2026-11-12T17:08:31Z", "2026-11-1AT17:08:31Z")).ok());
    CHECK(!ParseModelJson(Changed("\"generated_utc\": \"2026-11-12T17:08:31Z\"", "\"generated_utc\": 5")).ok());
}

EWS_TEST(ModelRejectsEmptyAndBrokenFiles) {
    CHECK(!ParseModelJson("").ok());
    CHECK(!ParseModelJson("[]").ok());
    CHECK(!ParseModelJson("{").ok());
    CHECK(!ParseModelJson(ews_test::SpecModelText() + "x").ok());
}
