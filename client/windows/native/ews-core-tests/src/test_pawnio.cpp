// test_pawnio.cpp: tests of the PawnIO source result in RunScan (fake source) and of the version text check.
#include <string>

#include "ews/pawnio.h"
#include "ews/scan.h"
#include "ews/scan_output.h"
#include "fake_sources.h"
#include "test_common.h"

using ews::PawnIoDevice;
using ews::PawnIoRegistry;
using ews::PawnIoState;
using ews::Tri;
using ews_test::Scenario;

namespace {

/// Builds a PawnIO state for a test.
PawnIoState State(PawnIoRegistry registry, const char* version, PawnIoDevice device, unsigned long error) {
    PawnIoState state;
    state.registry = registry;
    if (version != nullptr) {
        state.versionText = std::string(version);
    }
    state.device = device;
    state.deviceError = error;
    return state;
}

/// Runs the scan of a scenario whose PawnIO source gives this state.
ews::ScanResult RunWith(const PawnIoState& state) {
    Scenario s;
    s.pawnIo.result = ews::Reading<PawnIoState>::Success(state);
    return ews::RunScan(s.Sources(), ews::ScanContext::User, "2026-11-14T10:00:00Z");
}

}  // namespace

EWS_TEST(PawnIoNotInstalled) {
    auto r = RunWith(State(PawnIoRegistry::Missing, nullptr, PawnIoDevice::NotFound, 2));
    CHECK(r.pawnIo.common.present == Tri::False);
    CHECK(r.pawnIo.common.readable == Tri::Unknown);
    CHECK(r.pawnIo.common.live == Tri::Unknown);
    CHECK(r.pawnIo.common.error == 2);
    CHECK(!r.pawnIo.version.has_value());
}

EWS_TEST(PawnIoInstalledAndOpened) {
    auto r = RunWith(State(PawnIoRegistry::Found, "2.1.0", PawnIoDevice::Opened, 0));
    CHECK(r.pawnIo.common.present == Tri::True);
    CHECK(r.pawnIo.common.readable == Tri::True);
    CHECK(r.pawnIo.common.error == 0);
    CHECK(r.pawnIo.version.value_or("") == "2.1.0");
}

EWS_TEST(PawnIoInstalledAndDenied) {
    auto r = RunWith(State(PawnIoRegistry::Found, "2.1.0", PawnIoDevice::Denied, 5));
    CHECK(r.pawnIo.common.present == Tri::True);
    CHECK(r.pawnIo.common.readable == Tri::False);
    CHECK(r.pawnIo.common.error == 5);
}

EWS_TEST(PawnIoRegistryMissingButDeviceDeniedIsPresent) {
    auto r = RunWith(State(PawnIoRegistry::Missing, nullptr, PawnIoDevice::Denied, 5));
    CHECK(r.pawnIo.common.present == Tri::True);
    CHECK(r.pawnIo.common.readable == Tri::False);
    CHECK(!r.pawnIo.version.has_value());
}

EWS_TEST(PawnIoRegistryMissingButDeviceOpenedIsReadable) {
    auto r = RunWith(State(PawnIoRegistry::Missing, nullptr, PawnIoDevice::Opened, 0));
    CHECK(r.pawnIo.common.present == Tri::True);
    CHECK(r.pawnIo.common.readable == Tri::True);
}

EWS_TEST(PawnIoInstalledButDeviceNotFoundIsNotReadable) {
    auto r = RunWith(State(PawnIoRegistry::Found, "2.1.0", PawnIoDevice::NotFound, 2));
    CHECK(r.pawnIo.common.present == Tri::True);
    CHECK(r.pawnIo.common.readable == Tri::False);
}

EWS_TEST(PawnIoUnreadableRegistryAndNotFoundDeviceIsUnknown) {
    auto r = RunWith(State(PawnIoRegistry::Unknown, nullptr, PawnIoDevice::NotFound, 2));
    CHECK(r.pawnIo.common.present == Tri::Unknown);
    CHECK(r.pawnIo.common.readable == Tri::Unknown);
}

EWS_TEST(PawnIoUnexpectedDeviceErrorKeepsTheCode) {
    auto missing = RunWith(State(PawnIoRegistry::Missing, nullptr, PawnIoDevice::Other, 31));
    CHECK(missing.pawnIo.common.present == Tri::Unknown);
    CHECK(missing.pawnIo.common.error == 31);
    auto found = RunWith(State(PawnIoRegistry::Found, "2.1.0", PawnIoDevice::Other, 31));
    CHECK(found.pawnIo.common.present == Tri::True);
    CHECK(found.pawnIo.common.readable == Tri::Unknown);
    CHECK(found.pawnIo.common.error == 31);
}

EWS_TEST(PawnIoBadVersionTextIsLeftOut) {
    auto letter = RunWith(State(PawnIoRegistry::Found, "2.1.0a", PawnIoDevice::Opened, 0));
    CHECK(letter.pawnIo.common.present == Tri::True);
    CHECK(!letter.pawnIo.version.has_value());
    auto longText = RunWith(State(PawnIoRegistry::Found, "1.2.3.4.5.6.7.8.9.1.2.3.4.5.6.7.8", PawnIoDevice::Opened, 0));
    CHECK(!longText.pawnIo.version.has_value());
    auto empty = RunWith(State(PawnIoRegistry::Found, "", PawnIoDevice::Opened, 0));
    CHECK(!empty.pawnIo.version.has_value());
}

EWS_TEST(PawnIoFailedReadLeavesEverythingUnknown) {
    Scenario s;
    s.pawnIo.result = ews::Reading<PawnIoState>::Failure(ews::Status::Error, 1450);
    auto r = ews::RunScan(s.Sources(), ews::ScanContext::User, "2026-11-14T10:00:00Z");
    CHECK(r.pawnIo.common.present == Tri::Unknown);
    CHECK(r.pawnIo.common.readable == Tri::Unknown);
    CHECK(r.pawnIo.common.error == 1450);
    CHECK(!r.pawnIo.version.has_value());
    CHECK(r.level == ews::MeasurementLevel::L2);
}

EWS_TEST(PawnIoDoesNotChangeLevelAndTheDevicePathIsNotInTheFiles) {
    auto r = RunWith(State(PawnIoRegistry::Found, "2.1.0", PawnIoDevice::Opened, 0));
    CHECK(r.level == ews::MeasurementLevel::L2);
    auto json = ews::WriteCapabilitiesJson(r);
    auto log = ews::WriteScanLog(r);
    CHECK(json.find("GLOBALROOT") == std::string::npos);
    CHECK(log.find("GLOBALROOT") == std::string::npos);
    CHECK(json.find("\"id\": \"pawnio\", \"present\": true, \"readable\": true, \"live\": null, \"error\": 0, \"version\": \"2.1.0\"") !=
          std::string::npos);
}

EWS_TEST(PawnIoVersionCheckAcceptsDigitsAndDotsOnly) {
    CHECK(ews::IsValidPawnIoVersion("2.1.0"));
    CHECK(ews::IsValidPawnIoVersion("7"));
    CHECK(ews::IsValidPawnIoVersion(std::string(32, '1')));
    CHECK(!ews::IsValidPawnIoVersion(""));
    CHECK(!ews::IsValidPawnIoVersion(std::string(33, '1')));
    CHECK(!ews::IsValidPawnIoVersion("2.1.0-beta"));
    CHECK(!ews::IsValidPawnIoVersion("2 1"));
    CHECK(!ews::IsValidPawnIoVersion("\"2\""));
}
