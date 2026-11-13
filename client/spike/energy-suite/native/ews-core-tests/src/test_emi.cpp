// test_emi.cpp: tests of the pure EMI functions: metadata parser, channel classifier, power and unit conversion.
// The metadata fixtures are built byte by byte, as a driver would return them (EMI structures of emi.h).
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

#include "ews/emi.h"
#include "test_common.h"

using ews::EmiKind;
using ews::EmiSample;
using ews::Status;

namespace {

using Bytes = std::vector<unsigned char>;

constexpr std::size_t kNameFieldBytes = 32;  // EMI_NAME_MAX (16) wide characters

/// Appends a 16-bit value, low byte first.
void PutU16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<unsigned char>(value & 0xFF));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
}

/// Appends a 32-bit value, low byte first.
void PutU32(Bytes& bytes, std::uint32_t value) {
    PutU16(bytes, static_cast<std::uint16_t>(value & 0xFFFF));
    PutU16(bytes, static_cast<std::uint16_t>(value >> 16));
}

/// Appends the wide characters of a name, then a terminator when asked.
void PutName(Bytes& bytes, const wchar_t* name, bool terminator) {
    std::size_t length = std::wcslen(name);
    for (std::size_t i = 0; i < length; ++i) {
        PutU16(bytes, static_cast<std::uint16_t>(name[i]));
    }
    if (terminator) {
        PutU16(bytes, 0);
    }
}

/// @return The correct ChannelNameSize in bytes for a name with a terminator.
std::uint16_t NameBytes(const wchar_t* name) { return static_cast<std::uint16_t>((std::wcslen(name) + 1) * 2); }

/// Appends the version 2 header: OEM, model, revision and the channel count.
void PutHeaderV2(Bytes& bytes, std::uint16_t count) {
    bytes.insert(bytes.end(), 2 * kNameFieldBytes, 0);
    PutU16(bytes, 1);  // hardware revision
    PutU16(bytes, count);
}

/// Appends one version 2 channel with an explicit unit and name size field.
void PutChannelV2(Bytes& bytes, std::uint32_t unit, std::uint16_t sizeField, const wchar_t* name, bool terminator) {
    PutU32(bytes, unit);
    PutU16(bytes, sizeField);
    PutName(bytes, name, terminator);
}

/// Appends one correct version 2 channel.
void PutGoodChannelV2(Bytes& bytes, const wchar_t* name) { PutChannelV2(bytes, 0, NameBytes(name), name, true); }

/// @return A valid version 2 buffer with the four channels of the test PC.
Bytes FourChannelBuffer() {
    Bytes bytes;
    PutHeaderV2(bytes, 4);
    PutGoodChannelV2(bytes, L"RAPL_Package0_PKG");
    PutGoodChannelV2(bytes, L"RAPL_Package0_PP0");
    PutGoodChannelV2(bytes, L"RAPL_Package0_PP1");
    PutGoodChannelV2(bytes, L"RAPL_Package0_DRAM");
    return bytes;
}

/// @return A version 1 buffer: unit, OEM, model, revision, name size, name.
Bytes VersionOneBuffer(std::uint16_t sizeField, const wchar_t* name, bool terminator) {
    Bytes bytes;
    PutU32(bytes, 0);
    bytes.insert(bytes.end(), 2 * kNameFieldBytes, 0);
    PutU16(bytes, 1);
    PutU16(bytes, sizeField);
    PutName(bytes, name, terminator);
    return bytes;
}

/// @return True when the parser rejected the buffer with Error and returned no channels.
bool Rejected(std::uint16_t version, const Bytes& bytes) {
    auto result = ews::ParseEmiMetadata(version, bytes);
    return result.status == Status::Error && result.value.empty();
}

/// @return A known sample.
EmiSample Sample(std::uint64_t energy, std::uint64_t time) { return EmiSample{true, energy, time}; }

}  // namespace

EWS_TEST(EmiParseVersion2FourChannels) {
    Bytes bytes = FourChannelBuffer();
    auto result = ews::ParseEmiMetadata(2, bytes);
    CHECK(result.status == Status::Ok);
    CHECK(result.value.size() == 4);
    CHECK(result.value.size() == 4 && result.value[0].name == "RAPL_Package0_PKG");
    CHECK(result.value.size() == 4 && result.value[1].name == "RAPL_Package0_PP0");
    CHECK(result.value.size() == 4 && result.value[2].name == "RAPL_Package0_PP1");
    CHECK(result.value.size() == 4 && result.value[3].name == "RAPL_Package0_DRAM");
    CHECK(result.value.size() == 4 && result.value[3].unit == ews::EmiUnit::PicowattHours);
}

EWS_TEST(EmiParseVersion1OneChannel) {
    Bytes bytes = VersionOneBuffer(NameBytes(L"CPU"), L"CPU", true);
    auto result = ews::ParseEmiMetadata(1, bytes);
    CHECK(result.status == Status::Ok);
    CHECK(result.value.size() == 1);
    CHECK(result.value.size() == 1 && result.value[0].name == "CPU");
}

EWS_TEST(EmiParseRejectsShortBuffers) {
    CHECK(Rejected(2, Bytes()));
    CHECK(Rejected(2, Bytes(10, 0)));
    CHECK(Rejected(2, Bytes(67, 0)));  // one byte short of the version 2 header
    CHECK(Rejected(1, Bytes(10, 0)));
    CHECK(Rejected(1, Bytes(71, 0)));  // one byte short of the version 1 header
}

EWS_TEST(EmiParseRejectsBadChannelCount) {
    Bytes zero;
    PutHeaderV2(zero, 0);
    CHECK(Rejected(2, zero));
    Bytes tooMany;
    PutHeaderV2(tooMany, 257);
    PutGoodChannelV2(tooMany, L"A");
    CHECK(Rejected(2, tooMany));
}

EWS_TEST(EmiParseRejectsCountLargerThanBuffer) {
    Bytes bytes;
    PutHeaderV2(bytes, 3);  // says three channels, holds one
    PutGoodChannelV2(bytes, L"A");
    CHECK(Rejected(2, bytes));
}

EWS_TEST(EmiParseRejectsNameThatRunsPastTheBuffer) {
    Bytes bytes;
    PutHeaderV2(bytes, 1);
    PutChannelV2(bytes, 0, 200, L"ABC", true);  // size field 200, only 8 bytes follow
    CHECK(Rejected(2, bytes));
}

EWS_TEST(EmiParseRejectsOddAndZeroNameSize) {
    Bytes odd;
    PutHeaderV2(odd, 1);
    PutChannelV2(odd, 0, 7, L"ABC", true);
    CHECK(Rejected(2, odd));
    Bytes zero;
    PutHeaderV2(zero, 1);
    PutChannelV2(zero, 0, 0, L"", false);
    CHECK(Rejected(2, zero));
}

EWS_TEST(EmiParseRejectsNameWithoutTerminator) {
    Bytes bytes;
    PutHeaderV2(bytes, 1);
    PutChannelV2(bytes, 0, 6, L"ABC", false);  // three characters, size 6, no terminator
    CHECK(Rejected(2, bytes));
}

EWS_TEST(EmiParseRejectsBadVersionAndUnit) {
    Bytes good = FourChannelBuffer();
    CHECK(Rejected(0, good));
    CHECK(Rejected(3, good));
    Bytes unit;
    PutHeaderV2(unit, 1);
    PutChannelV2(unit, 9, NameBytes(L"A"), L"A", true);
    CHECK(Rejected(2, unit));
}

EWS_TEST(EmiParseVersion1BadName) {
    CHECK(Rejected(1, VersionOneBuffer(0, L"", false)));
    CHECK(Rejected(1, VersionOneBuffer(99, L"CPU", true)));
    CHECK(Rejected(1, VersionOneBuffer(5, L"CPU", true)));
    CHECK(Rejected(1, VersionOneBuffer(6, L"CPU", false)));
}

EWS_TEST(EmiClassifyRaplNames) {
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_PKG") == EmiKind::CpuPackage);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_PP0") == EmiKind::CpuCores);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_PP1") == EmiKind::Igpu);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_DRAM") == EmiKind::Dram);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package12_PKG") == EmiKind::CpuPackage);
    CHECK(ews::ClassifyEmiChannel("rapl_package1_pkg") == EmiKind::CpuPackage);
}

EWS_TEST(EmiClassifySystemWords) {
    CHECK(ews::ClassifyEmiChannel("PSYS") == EmiKind::System);
    CHECK(ews::ClassifyEmiChannel("SYSTEM_TOTAL") == EmiKind::System);
    CHECK(ews::ClassifyEmiChannel("platform power") == EmiKind::System);
    CHECK(ews::ClassifyEmiChannel("SUBSYSTEMX") == EmiKind::Other);
    CHECK(ews::ClassifyEmiChannel("PSYS0") == EmiKind::Other);  // a digit is not a word boundary
}

EWS_TEST(EmiClassifyCpuAndOthers) {
    CHECK(ews::ClassifyEmiChannel("CPU") == EmiKind::CpuPackage);
    CHECK(ews::ClassifyEmiChannel("cpu") == EmiKind::CpuPackage);
    CHECK(ews::ClassifyEmiChannel("CPU_BIG") == EmiKind::CpuCores);
    CHECK(ews::ClassifyEmiChannel("GPU") == EmiKind::Other);
    CHECK(ews::ClassifyEmiChannel("") == EmiKind::Other);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package_PKG") == EmiKind::Other);  // no package number
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_PKGX") == EmiKind::Other);
}

EWS_TEST(EmiKindWords) {
    CHECK(std::string(ews::EmiKindWord(EmiKind::System)) == "system");
    CHECK(std::string(ews::EmiKindWord(EmiKind::CpuPackage)) == "cpu_package");
    CHECK(std::string(ews::EmiKindWord(EmiKind::Dram)) == "dram");
    CHECK(std::string(ews::EmiKindWord(EmiKind::CpuCores)) == "cpu_cores");
    CHECK(std::string(ews::EmiKindWord(EmiKind::Igpu)) == "igpu");
    CHECK(std::string(ews::EmiKindWord(EmiKind::Other)) == "other");
}

EWS_TEST(EmiPowerNormal) {
    // 1 000 000 000 pWh = 3.6 J, over 1 s (10 000 000 units of 100 ns) = 3.6 W.
    auto r = ews::EmiPowerWatts(Sample(5, 100), Sample(5 + 1000000000ULL, 100 + 10000000ULL));
    CHECK(r.status == Status::Ok);
    CHECK(std::fabs(r.value - 3.6) < 1e-9);
}

EWS_TEST(EmiPowerZeroAndNegativeTimeDelta) {
    CHECK(ews::EmiPowerWatts(Sample(0, 100), Sample(10, 100)).status == Status::Unavailable);
    CHECK(ews::EmiPowerWatts(Sample(0, 100), Sample(10, 50)).status == Status::Unavailable);
}

EWS_TEST(EmiPowerBackwardsEnergyAndUnknownSample) {
    CHECK(ews::EmiPowerWatts(Sample(100, 0), Sample(50, 10000000)).status == Status::Unavailable);
    CHECK(ews::EmiPowerWatts(EmiSample{}, Sample(50, 10000000)).status == Status::Unavailable);
    CHECK(ews::EmiPowerWatts(Sample(0, 0), EmiSample{}).status == Status::Unavailable);
}

EWS_TEST(EmiPowerSameEnergyIsZeroWatts) {
    auto r = ews::EmiPowerWatts(Sample(70, 0), Sample(70, 10000000));
    CHECK(r.status == Status::Ok);
    CHECK(r.value == 0.0);
}

EWS_TEST(PicowattHoursNormalAndZero) {
    auto thousand = ews::PicowattHoursToMicrojoules(1000000);  // 1 000 000 pWh = 3600 uJ
    CHECK(thousand.status == Status::Ok);
    CHECK(thousand.value == 3600);
    auto zero = ews::PicowattHoursToMicrojoules(0);
    CHECK(zero.status == Status::Ok);
    CHECK(zero.value == 0);
}

EWS_TEST(PicowattHoursRoundsToNearest) {
    CHECK(ews::PicowattHoursToMicrojoules(138).value == 0);  // 0.4968 uJ
    CHECK(ews::PicowattHoursToMicrojoules(139).value == 1);  // 0.5004 uJ
    CHECK(ews::PicowattHoursToMicrojoules(1).value == 0);
}

EWS_TEST(PicowattHoursLargestInput) {
    // 18446744073709551615 * 0.0036 = 66408278665354385.8, so it fits and rounds up. It cannot overflow.
    auto r = ews::PicowattHoursToMicrojoules(UINT64_MAX);
    CHECK(r.status == Status::Ok);
    CHECK(r.value == 66408278665354386ULL);
}
