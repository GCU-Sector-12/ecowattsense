// test_web_setup.cpp: tests of the pure parts of the web installer (ews/web_setup.h). No network is used.
#include <string>

#include "ews/web_setup.h"
#include "temp_folder.h"
#include "test_common.h"

using ews::IsAllowedRedirectUrl;
using ews::ParseWebManifest;
using ews::Sha256Matches;
using ews::SizeMatches;
using ews::WebSetupResult;

namespace {

const char kHash[] = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

/// @param version The version value.
/// @param url The zip_url value.
/// @param sha The sha256 value.
/// @param size The size value as JSON text.
/// @return A manifest document.
std::string Manifest(const std::string& version, const std::string& url, const std::string& sha, const std::string& size) {
    return "{\"version\":\"" + version + "\",\"zip_url\":\"" + url + "\",\"sha256\":\"" + sha + "\",\"size\":" + size + "}";
}

/// @return A valid package address.
std::string GoodUrl() { return std::string(ews::kWebSetupZipUrlPrefix) + "v0.1.0/EcoWattSense-0.1.0.zip"; }

}  // namespace

EWS_TEST(WebManifestAccepted) {
    auto r = ParseWebManifest(Manifest("0.1.0", GoodUrl(), "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD", "1234"));
    CHECK(r.ok());
    CHECK(r.value.version == "0.1.0");
    CHECK(r.value.zipUrl == GoodUrl());
    CHECK(r.value.sha256 == kHash);
    CHECK(r.value.size == 1234);
}

EWS_TEST(WebManifestBadVersion) {
    CHECK(!ParseWebManifest(Manifest("", GoodUrl(), kHash, "10")).ok());
    CHECK(!ParseWebManifest(Manifest("0.1.0-beta", GoodUrl(), kHash, "10")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0 ", GoodUrl(), kHash, "10")).ok());
}

EWS_TEST(WebManifestBadUrl) {
    CHECK(!ParseWebManifest(Manifest("1.0", "http://github.com/GCU-Sector-12/ecowattsense/releases/download/a.zip", kHash, "10")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", "https://github.com/other/repo/releases/download/a.zip", kHash, "10")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", std::string(ews::kWebSetupZipUrlPrefix), kHash, "10")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl() + " x", kHash, "10")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl() + std::string(600, 'a'), kHash, "10")).ok());
}

EWS_TEST(WebManifestBadHash) {
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl(), "abc", "10")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl(), std::string(63, 'a') + "g", "10")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl(), std::string(65, 'a'), "10")).ok());
}

EWS_TEST(WebManifestBadSize) {
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl(), kHash, "0")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl(), kHash, "-5")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl(), kHash, "1.5")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl(), kHash, "314572801")).ok());
    CHECK(ParseWebManifest(Manifest("1.0", GoodUrl(), kHash, "314572800")).ok());
    CHECK(ParseWebManifest(Manifest("1.0", GoodUrl(), kHash, "1")).ok());
    CHECK(!ParseWebManifest(Manifest("1.0", GoodUrl(), kHash, "\"10\"")).ok());
}

EWS_TEST(WebManifestBadShape) {
    CHECK(!ParseWebManifest("").ok());
    CHECK(!ParseWebManifest("[]").ok());
    CHECK(!ParseWebManifest("{").ok());
    CHECK(!ParseWebManifest("{\"version\":\"1.0\"}").ok());
    CHECK(!ParseWebManifest("{\"version\":1,\"zip_url\":\"x\",\"sha256\":\"y\",\"size\":2}").ok());
}

EWS_TEST(WebRedirectHosts) {
    CHECK(IsAllowedRedirectUrl("https://github.com/a/b"));
    CHECK(IsAllowedRedirectUrl("https://objects.githubusercontent.com/x?y=1"));
    CHECK(IsAllowedRedirectUrl("https://release-assets.githubusercontent.com/z"));
    CHECK(IsAllowedRedirectUrl("https://GitHub.com"));
    CHECK(!IsAllowedRedirectUrl("http://github.com/a"));
    CHECK(!IsAllowedRedirectUrl("https://evil.com/a"));
    CHECK(!IsAllowedRedirectUrl("https://github.com.evil.com/a"));
    CHECK(!IsAllowedRedirectUrl("https://evilgithub.com/a"));
    CHECK(!IsAllowedRedirectUrl("https://github.com@evil.com/a"));
    CHECK(!IsAllowedRedirectUrl("https://evil.com@github.com/a"));
    CHECK(!IsAllowedRedirectUrl("https://github.com:8443/a"));
    CHECK(!IsAllowedRedirectUrl("https://github.com\\@evil.com/"));
    CHECK(!IsAllowedRedirectUrl("//github.com/a"));
    CHECK(!IsAllowedRedirectUrl("/a/b"));
    CHECK(!IsAllowedRedirectUrl("https://"));
    CHECK(!IsAllowedRedirectUrl(""));
    CHECK(!IsAllowedRedirectUrl("ftp://github.com/a"));
    CHECK(!IsAllowedRedirectUrl("https://github.com/a b"));
}

EWS_TEST(WebSizeAndHashCompare) {
    CHECK(SizeMatches(10, 10));
    CHECK(!SizeMatches(9, 10));
    CHECK(!SizeMatches(11, 10));
    CHECK(Sha256Matches(kHash, kHash));
    CHECK(Sha256Matches(kHash, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"));
    CHECK(!Sha256Matches(kHash, "aa7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(!Sha256Matches(kHash, "ba78"));
    CHECK(!Sha256Matches("", ""));
}

EWS_TEST(WebSha256OfFile) {
    auto folder = ews_test::NewDataFolder("websha");
    ews_test::WriteText(folder / "abc.bin", "abc");
    auto r = ews::Sha256HexOfFile((folder / "abc.bin").c_str());
    CHECK(r.ok());
    CHECK(r.value == kHash);
    CHECK(Sha256Matches(r.value, kHash));
    ews_test::WriteText(folder / "empty.bin", "");
    auto empty = ews::Sha256HexOfFile((folder / "empty.bin").c_str());
    CHECK(empty.ok());
    CHECK(empty.value == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    auto missing = ews::Sha256HexOfFile((folder / "none.bin").c_str());
    CHECK(!missing.ok());
    ews_test::RemoveDataFolder(folder);
}

EWS_TEST(WebSetupExitCodesAndMessages) {
    CHECK(ews::WebSetupExitCode(WebSetupResult::Ok) == 0);
    CHECK(ews::WebSetupExitCode(WebSetupResult::Cancelled) == 1);
    CHECK(ews::WebSetupExitCode(WebSetupResult::NotStarted) == 11);
    CHECK(ews::WebSetupExitCode(WebSetupResult::Internal) == 12);
    CHECK(std::wstring(ews::WebSetupMessage(WebSetupResult::HashMismatch)) == L"The download is damaged. Try again.");
    CHECK(std::wstring(ews::WebSetupMessage(WebSetupResult::NotStarted)) == L"Setup was not started.");
    CHECK(std::wstring(ews::WebSetupMessage(WebSetupResult::Extract)).find(L"http") == std::wstring::npos);
    CHECK(std::wstring(ews::WebSetupMessage(WebSetupResult::Ok)).empty());
}

EWS_TEST(WebServerUrlCheck) {
    CHECK(ews::IsValidServerUrl("https://api.example.com"));
    CHECK(ews::IsValidServerUrl("https://api.example.com/path?a=1"));
    CHECK(!ews::IsValidServerUrl("http://api.example.com"));
    CHECK(!ews::IsValidServerUrl("https://"));
    CHECK(!ews::IsValidServerUrl(""));
    CHECK(!ews::IsValidServerUrl("https://a.com/x y"));
    CHECK(!ews::IsValidServerUrl("https://a.com/x\"y"));
    CHECK(!ews::IsValidServerUrl("https://a.com/x'y"));
    CHECK(!ews::IsValidServerUrl("https://a.com/x\ny"));
    CHECK(ews::IsValidServerUrl("https://a.com/" + std::string(186, 'a')));
    CHECK(!ews::IsValidServerUrl("https://a.com/" + std::string(187, 'a')));
}

EWS_TEST(WebManifestServerUrlOptional) {
    std::string base = Manifest("1.0", GoodUrl(), kHash, "10");
    base.pop_back();
    auto good = ParseWebManifest(base + ",\"server_url\":\"https://api.example.com\"}");
    CHECK(good.ok());
    CHECK(good.value.serverUrl == "https://api.example.com");
    auto bad = ParseWebManifest(base + ",\"server_url\":\"http://x.com\"}");
    CHECK(bad.ok());
    CHECK(bad.value.serverUrl.empty());
    auto wrongType = ParseWebManifest(base + ",\"server_url\":5}");
    CHECK(wrongType.ok());
    CHECK(wrongType.value.serverUrl.empty());
    auto missing = ParseWebManifest(base + "}");
    CHECK(missing.ok());
    CHECK(missing.value.serverUrl.empty());
}
