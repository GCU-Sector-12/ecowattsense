// test_cpu_facts.cpp: tests of the CPUID signature decoding and the physical core count (SPEC 1.2).
#include <cstdint>
#include <vector>

#include "ews/cpu_facts.h"
#include "test_common.h"

namespace {

using Bytes = std::vector<std::uint8_t>;

/// Appends one 32-bit value, low byte first.
void PutU32(Bytes& bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    }
}

/// Appends one record: relationship, size and filler bytes up to the size.
void PutRecord(Bytes& bytes, std::uint32_t relationship, std::uint32_t size) {
    PutU32(bytes, relationship);
    PutU32(bytes, size);
    for (std::uint32_t i = 8; i < size; ++i) {
        bytes.push_back(0);
    }
}

}  // namespace

EWS_TEST(CpuSignatureOfAZen2Laptop) {
    // Ryzen 7 4700U: family 0x17 (base 0xF + extended 8), model 0x60 (extended 6, base 0), stepping 1.
    auto sig = ews::DecodeCpuSignature(0x00860F01);
    CHECK(sig.has_value());
    CHECK(sig.value_or(ews::CpuSignature{}).family == 0x17);
    CHECK(sig.value_or(ews::CpuSignature{}).model == 0x60);
    CHECK(sig.value_or(ews::CpuSignature{}).stepping == 1);
}

EWS_TEST(CpuSignatureOfAnIntelCore) {
    // Family 6, model 0x8C (extended 8, base C), stepping 1.
    auto sig = ews::DecodeCpuSignature(0x000806C1);
    CHECK(sig.has_value());
    CHECK(sig.value_or(ews::CpuSignature{}).family == 6);
    CHECK(sig.value_or(ews::CpuSignature{}).model == 0x8C);
    CHECK(sig.value_or(ews::CpuSignature{}).stepping == 1);
}

EWS_TEST(CpuSignatureIgnoresExtendedFieldsForOtherFamilies) {
    // Base family 5: neither extended field applies, even when the bits are set.
    auto sig = ews::DecodeCpuSignature(0x08F50532);
    CHECK(sig.has_value());
    CHECK(sig.value_or(ews::CpuSignature{}).family == 5);
    CHECK(sig.value_or(ews::CpuSignature{}).model == 0x3);
    CHECK(sig.value_or(ews::CpuSignature{}).stepping == 2);
}

EWS_TEST(CpuSignatureZeroIsUnknown) { CHECK(!ews::DecodeCpuSignature(0).has_value()); }

EWS_TEST(CoreCountOfThreeRecords) {
    Bytes bytes;
    PutRecord(bytes, 0, 48);
    PutRecord(bytes, 0, 48);
    PutRecord(bytes, 0, 48);
    CHECK(ews::CountProcessorCores(bytes).value_or(0) == 3);
}

EWS_TEST(CoreCountHandlesRecordsOfDifferentSize) {
    Bytes bytes;
    PutRecord(bytes, 0, 40);
    PutRecord(bytes, 0, 72);
    CHECK(ews::CountProcessorCores(bytes).value_or(0) == 2);
}

EWS_TEST(CoreCountIsUnknownForBadBuffers) {
    CHECK(!ews::CountProcessorCores(Bytes{}).has_value());
    Bytes tooShort(7, 0);
    CHECK(!ews::CountProcessorCores(tooShort).has_value());
    Bytes zeroSize;
    PutRecord(zeroSize, 0, 8);
    zeroSize[4] = 0;  // Size = 0 would never advance
    CHECK(!ews::CountProcessorCores(zeroSize).has_value());
    Bytes pastEnd;
    PutRecord(pastEnd, 0, 48);
    pastEnd[4] = 100;  // Size beyond the buffer
    CHECK(!ews::CountProcessorCores(pastEnd).has_value());
    Bytes trailing;
    PutRecord(trailing, 0, 48);
    trailing.push_back(1);  // a stray byte after the last record
    CHECK(!ews::CountProcessorCores(trailing).has_value());
}

EWS_TEST(CoreCountIsUnknownWhenARecordIsNotACoreRecord) {
    Bytes bytes;
    PutRecord(bytes, 0, 48);
    PutRecord(bytes, 3, 48);
    CHECK(!ews::CountProcessorCores(bytes).has_value());
}
