// test_device_name.cpp: tests of the pure name rules (ews/device_name.h): the pipe message, base64, the name check,
// the name-sync state file and the PUT answer. No file or network is used.
#include <string>

#include "ews/device_name.h"
#include "test_common.h"

using ews::CheckedDeviceName;
using ews::DecodeBase64;
using ews::NameSync;
using ews::ParseDeviceAnswer;
using ews::ParseNameMessage;
using ews::ParseNameSync;

EWS_TEST(NameMessageGoodGivesTheName) {
    auto r = ParseNameMessage("EWS1 name=TGFiIFBD\n");
    CHECK(r.ok());
    CHECK(r.value.has_value() && *r.value == "Lab PC");
}

EWS_TEST(NameMessageEmptyValueMeansNoName) {
    auto r = ParseNameMessage("EWS1 name=\n");
    CHECK(r.ok());
    CHECK(!r.value.has_value());
}

EWS_TEST(NameMessageTrimsSpacesAtBothEnds) {
    auto r = ParseNameMessage("EWS1 name=ICBMYWIgIA==\n");
    CHECK(r.ok() && r.value.has_value() && *r.value == "Lab");
}

EWS_TEST(NameMessageKeepsMultiByteCharacters) {
    auto r = ParseNameMessage("EWS1 name=Q2Fmw6k=\n");
    CHECK(r.ok() && r.value.has_value() && *r.value == "Caf\xc3\xa9");
}

EWS_TEST(NameMessageNeedsPrefixAndLineFeed) {
    CHECK(!ParseNameMessage("EWS1 name=TGFiIFBD").ok());
    CHECK(!ParseNameMessage("EWS1 nam=TGFiIFBD\n").ok());
    CHECK(!ParseNameMessage("EWS1  name=TGFiIFBD\n").ok());
    CHECK(!ParseNameMessage("").ok());
    CHECK(!ParseNameMessage("EWS1 name=TGFiIFBD\r\n").ok());
}

EWS_TEST(NameMessageOverTheByteLimitIsRefused) {
    // 'YWFh' is "aaa": the text is good base64, so only the size or the name length can refuse it.
    std::string group = "YWFh";
    std::string longValue;
    for (int i = 0; i < 61; ++i) {
        longValue += group;
    }
    CHECK(!ParseNameMessage("EWS1 name=" + longValue + "\n").ok());  // 255 bytes, 183 characters
    CHECK(!ParseNameMessage("EWS1 name=" + longValue + group + group + "\n").ok());  // 263 bytes
    CHECK(!ParseNameMessage("EWS1 name=" + std::string(300, 'A') + "\n").ok());
}

EWS_TEST(NameMessageRefusesBadNames) {
    CHECK(!ParseNameMessage("EWS1 name=TGFiAVBD\n").ok());   // control character 0x01
    CHECK(!ParseNameMessage("EWS1 name=TGFiClBD\n").ok());   // line feed inside
    CHECK(!ParseNameMessage("EWS1 name=Qf8=\n").ok());       // not UTF-8
    CHECK(!ParseNameMessage("EWS1 name=wIA=\n").ok());       // overlong
    CHECK(!ParseNameMessage("EWS1 name=7aCA\n").ok());       // surrogate U+D800
    CHECK(!ParseNameMessage("EWS1 name=IA==\n").ok());       // only a space
    CHECK(!ParseNameMessage("EWS1 name=wpU=\n").ok());       // C1 control U+0095
}

EWS_TEST(NameMessageRefusesBadBase64) {
    CHECK(!ParseNameMessage("EWS1 name=TGFi*FBD\n").ok());
    CHECK(!ParseNameMessage("EWS1 name=TGFiIFB\n").ok());      // length not a multiple of 4
    CHECK(!ParseNameMessage("EWS1 name=TGFiIF==\n").ok());     // padding where a full group stands: bits not zero or not canonical
    CHECK(!ParseNameMessage("EWS1 name=TGFi IFBD\n").ok());
    CHECK(!ParseNameMessage("EWS1 name====\n").ok());
}

EWS_TEST(Base64DecodeCases) {
    CHECK(DecodeBase64("TWFu") == std::optional<std::string>("Man"));
    CHECK(DecodeBase64("TWE=") == std::optional<std::string>("Ma"));
    CHECK(DecodeBase64("TQ==") == std::optional<std::string>("M"));
    CHECK(!DecodeBase64("TQ=").has_value());
    CHECK(!DecodeBase64("TR==").has_value());  // unused bits are not zero
    CHECK(!DecodeBase64("").has_value());
}

EWS_TEST(CheckedNameLimits) {
    CHECK(CheckedDeviceName(std::string(60, 'a')).has_value());
    CHECK(!CheckedDeviceName(std::string(61, 'a')).has_value());
    CHECK(CheckedDeviceName("  x  ") == std::optional<std::string>("x"));
    CHECK(!CheckedDeviceName("").has_value());
    CHECK(!CheckedDeviceName("a\tb").has_value());
    CHECK(!CheckedDeviceName("a\x7f").has_value());
}

EWS_TEST(NameSyncFileRoundTrip) {
    CHECK(ParseNameSync(ews::FormatNameSync(NameSync::Pending)) == NameSync::Pending);
    CHECK(ParseNameSync(ews::FormatNameSync(NameSync::Ok)) == NameSync::Ok);
    CHECK(ParseNameSync(ews::FormatNameSync(NameSync::Failed)) == NameSync::Failed);
    CHECK(ews::FormatNameSync(NameSync::Pending) == "{\"name_sync\":\"pending\"}\n");
    CHECK(ParseNameSync("{\"name_sync\":\"weird\"}") == NameSync::None);
    CHECK(ParseNameSync("not json") == NameSync::None);
    CHECK(ParseNameSync("{\"name_sync\":3}") == NameSync::None);
}

EWS_TEST(DeviceAnswerNameCases) {
    auto named = ParseDeviceAnswer("{\"device_id\":\"x\",\"device_name\":\"Lab\",\"created\":false}");
    CHECK(named.ok() && named.value.name == std::optional<std::string>("Lab"));
    auto none = ParseDeviceAnswer("{\"device_name\":null}");
    CHECK(none.ok() && !none.value.name.has_value());
    CHECK(!ParseDeviceAnswer("{}").ok());
    CHECK(!ParseDeviceAnswer("{\"device_name\":5}").ok());
    CHECK(!ParseDeviceAnswer("[").ok());
}
