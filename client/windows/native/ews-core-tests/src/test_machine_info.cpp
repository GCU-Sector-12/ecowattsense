// test_machine_info.cpp: tests of the SMBIOS parser and the chassis mapping, on byte fixtures.
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

#include "ews/machine_info.h"
#include "test_common.h"

using Bytes = std::vector<std::uint8_t>;

namespace {

/// Appends raw bytes to a fixture.
void Add(Bytes& out, std::initializer_list<std::uint8_t> bytes) { out.insert(out.end(), bytes); }

/// Appends one string of a string set with its NUL terminator.
void AddText(Bytes& out, std::string_view text) {
    out.insert(out.end(), text.begin(), text.end());
    out.push_back(0);
}

/// Type 1 (System Information): string 1 = maker, string 2 = product name, string 3 = serial (must never be read).
void AddType1(Bytes& out) {
    Add(out, {0x01, 0x1B, 0x01, 0x00, 0x01, 0x02, 0x00, 0x03});
    Add(out, {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA});
    Add(out, {0x06, 0x00, 0x00});  // wake-up type, SKU string index 0, family string index 0
    AddText(out, "ACME");
    AddText(out, "Model X");
    AddText(out, "SERIAL-123");
    out.push_back(0);  // end of the string set
}

/// Type 3 (Chassis): the type byte is at offset 5.
void AddType3(Bytes& out, std::uint8_t chassisByte) {
    Add(out, {0x03, 0x06, 0x02, 0x00, 0x00, chassisByte});
    Add(out, {0x00, 0x00});  // empty string set
}

/// Type 127 (end of table).
void AddEnd(Bytes& out) { Add(out, {0x7F, 0x04, 0x03, 0x00, 0x00, 0x00}); }

Bytes ValidTable(std::uint8_t chassisByte) {
    Bytes t;
    AddType1(t);
    AddType3(t, chassisByte);
    AddEnd(t);
    return t;
}

}  // namespace

EWS_TEST(SmbiosValid) {
    Bytes t = ValidTable(0x0A);
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(info.maker.has_value() && *info.maker == "ACME");
    CHECK(info.model.has_value() && *info.model == "Model X");
    CHECK(info.chassisType.has_value() && *info.chassisType == 10);
}

EWS_TEST(SmbiosChassisBit7Masked) {
    Bytes t = ValidTable(0x8A);
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(info.chassisType.has_value() && *info.chassisType == 10);
}

EWS_TEST(SmbiosEmptyBuffer) {
    ews::SmbiosInfo info = ews::ParseSmbios(std::span<const std::uint8_t>{});
    CHECK(!info.maker.has_value());
    CHECK(!info.model.has_value());
    CHECK(!info.chassisType.has_value());
}

EWS_TEST(SmbiosTruncatedInsideType1) {
    Bytes t = ValidTable(0x0A);
    t.resize(20);  // cut in the middle of the type 1 formatted area
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(!info.maker.has_value());
    CHECK(!info.chassisType.has_value());
}

EWS_TEST(SmbiosTruncatedAfterType1) {
    Bytes t;
    AddType1(t);
    AddType3(t, 0x0A);
    t.resize(t.size() - 4);  // type 3 loses its string set and part of its header
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(info.maker.has_value() && *info.maker == "ACME");
    CHECK(!info.chassisType.has_value());
}

EWS_TEST(SmbiosLengthBelowFour) {
    Bytes t;
    Add(t, {0x01, 0x02, 0x00, 0x00, 0x00, 0x00});  // length 2 is impossible
    AddType3(t, 0x0A);
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(!info.maker.has_value());
    CHECK(!info.chassisType.has_value());  // parsing stops at the bad structure
}

EWS_TEST(SmbiosBadStringIndex) {
    Bytes t;
    Add(t, {0x01, 0x08, 0x01, 0x00, 0x09, 0x02, 0x00, 0x00});  // maker index 9, but only 2 strings
    AddText(t, "ACME");
    AddText(t, "Model X");
    t.push_back(0);
    AddType3(t, 0x03);
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(!info.maker.has_value());
    CHECK(info.model.has_value() && *info.model == "Model X");
    CHECK(info.chassisType.has_value() && *info.chassisType == 3);
}

EWS_TEST(SmbiosMissingTerminator) {
    Bytes t;
    Add(t, {0x01, 0x08, 0x01, 0x00, 0x01, 0x02, 0x00, 0x00});
    AddText(t, "ACME");
    AddText(t, "Mod");  // the string set never ends: the final zero pair is missing
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(!info.maker.has_value());
    CHECK(!info.model.has_value());
}

EWS_TEST(SmbiosNoType3) {
    Bytes t;
    AddType1(t);
    AddEnd(t);
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(info.maker.has_value() && *info.maker == "ACME");
    CHECK(!info.chassisType.has_value());
}

EWS_TEST(SmbiosNoType1) {
    Bytes t;
    AddType3(t, 0x03);
    AddEnd(t);
    ews::SmbiosInfo info = ews::ParseSmbios(t);
    CHECK(!info.maker.has_value());
    CHECK(!info.model.has_value());
    CHECK(info.chassisType.has_value() && *info.chassisType == 3);
}

EWS_TEST(SmbiosExtractHeader) {
    Bytes raw = {0x00, 0x03, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    auto table = ews::ExtractSmbiosTable(raw);
    CHECK(table.size() == 4);  // the length field says 4
    Bytes tooShort = {0x00, 0x03};
    CHECK(ews::ExtractSmbiosTable(tooShort).empty());
    raw[4] = 0xFF;  // a length larger than the buffer is clamped
    CHECK(ews::ExtractSmbiosTable(raw).size() == 5);
}

EWS_TEST(ChassisWords) {
    CHECK(std::string_view(ews::ChassisWord(10)) == "laptop");
    CHECK(std::string_view(ews::ChassisWord(31)) == "laptop");
    CHECK(std::string_view(ews::ChassisWord(3)) == "desktop");
    CHECK(std::string_view(ews::ChassisWord(35)) == "desktop");
    CHECK(std::string_view(ews::ChassisWord(1)) == "other");
    CHECK(std::string_view(ews::ChassisWord(23)) == "other");
    CHECK(std::string_view(ews::ChassisWord(0x8A)) == "laptop");  // bit 7 set
}
