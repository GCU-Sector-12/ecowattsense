// test_signature_check.cpp: tests of the Setup.exe signature decision (ews/signature_check.h). No network, no certificate is made.
#include <windows.h>

#include <string>

#include "ews/signature_check.h"
#include "ews/web_setup.h"
#include "test_common.h"

using ews::DecideSignature;
using ews::SignatureVerdict;

namespace {

const char kPin[] = "0123456789abcdef0123456789abcdef01234567";
const char kOther[] = "ffffffffffffffffffffffffffffffffffffffff";

}  // namespace

EWS_TEST(SignatureValidAndMatching) {
    CHECK(DecideSignature(ews::kTrustOk, kPin, kPin) == SignatureVerdict::Start);
}

EWS_TEST(SignatureThumbprintCaseIgnored) {
    CHECK(DecideSignature(ews::kTrustOk, "0123456789ABCDEF0123456789ABCDEF01234567", kPin) == SignatureVerdict::Start);
}

EWS_TEST(SignatureUntrustedRootAndMatching) {
    CHECK(DecideSignature(ews::kTrustUntrustedRoot, kPin, kPin) == SignatureVerdict::Start);
    CHECK(DecideSignature(ews::kTrustChaining, kPin, kPin) == SignatureVerdict::Start);
}

EWS_TEST(SignatureValidButOtherThumbprint) {
    CHECK(DecideSignature(ews::kTrustOk, kOther, kPin) == SignatureVerdict::Refuse);
    CHECK(DecideSignature(ews::kTrustUntrustedRoot, kOther, kPin) == SignatureVerdict::Refuse);
}

EWS_TEST(SignatureMissing) {
    CHECK(DecideSignature(ews::kTrustNoSignature, "", kPin) == SignatureVerdict::Refuse);
    CHECK(DecideSignature(ews::kTrustOk, "", kPin) == SignatureVerdict::Refuse);
}

EWS_TEST(SignatureTampered) {
    CHECK(DecideSignature(ews::kTrustBadDigest, kPin, kPin) == SignatureVerdict::Refuse);
}

EWS_TEST(SignatureOtherFailureRefused) {
    CHECK(DecideSignature(static_cast<std::int32_t>(0x80092003u), kPin, kPin) == SignatureVerdict::Refuse);
}

EWS_TEST(SignaturePinMalformedRefused) {
    CHECK(DecideSignature(ews::kTrustOk, kPin, "abc") == SignatureVerdict::Refuse);
}

EWS_TEST(SignaturePlaceholderPinSkips) {
    CHECK(DecideSignature(ews::kTrustNoSignature, "", ews::kSigningThumbprintPlaceholder) == SignatureVerdict::Skipped);
    CHECK(DecideSignature(ews::kTrustOk, kPin, ews::kSigningThumbprintPlaceholder) == SignatureVerdict::Skipped);
}

EWS_TEST(SignatureOfUnsignedTestProgramRefused) {
    wchar_t self[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, self, MAX_PATH);
    CHECK(length > 0 && length < MAX_PATH);
    CHECK(ews::VerifySetupSignature(self, kPin) == SignatureVerdict::Refuse);
}

EWS_TEST(SignatureOfMissingFileRefused) {
    CHECK(ews::VerifySetupSignature(L"C:\\no-such-folder\\Setup.exe", kPin) == SignatureVerdict::Refuse);
}

EWS_TEST(SignatureOfFileWithPlaceholderPinSkipped) {
    CHECK(ews::VerifySetupSignature(L"C:\\no-such-folder\\Setup.exe", ews::kSigningThumbprintPlaceholder) == SignatureVerdict::Skipped);
}

EWS_TEST(SignatureRefusedExitCodeAndMessage) {
    CHECK(ews::WebSetupExitCode(ews::WebSetupResult::SignatureRefused) == 13);
    std::wstring message = ews::WebSetupMessage(ews::WebSetupResult::SignatureRefused);
    CHECK(message == L"The downloaded Setup is not signed by EcoWattSense. It was not started.");
}
