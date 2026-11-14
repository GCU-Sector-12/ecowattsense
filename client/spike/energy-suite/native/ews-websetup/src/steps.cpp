// steps.cpp: implementation of the web installer steps (steps.h, SPEC 13).
// Order: manifest, work folder, package download, size and SHA256 check, tar.exe extraction, ShellExecuteEx of Setup.exe.
// Failure paths: every step returns a WebSetupResult; nothing here throws. A failed or empty read means "stop", never "go on".
#include "steps.h"

#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>

#include <string>

#include "download.h"
#include "ews/handles.h"
#include "ews/web_setup.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace websetup {

namespace {

using ews::WebManifest;
using ews::WebSetupResult;

constexpr wchar_t kManifestUrl[] = L"https://raw.githubusercontent.com/GCU-Sector-12/ecowattsense/EWS-energy-suite/client/spike/energy-suite/release/latest.json";
constexpr std::uint64_t kManifestMaxBytes = 64 * 1024;
constexpr DWORD kExtractTimeoutMs = 10 * 60 * 1000;

/// Progress state of the package download.
struct PackageProgress {
    SetupHost* host = nullptr;
    std::uint64_t expected = 0;
    int lastPercent = -1;
};

/// Maps a download outcome to a step result.
/// @param outcome The outcome.
/// @param failure The result for every failure except a cancel.
/// @return Ok, Cancelled or the failure result.
WebSetupResult MapOutcome(DownloadOutcome outcome, WebSetupResult failure) {
    if (outcome == DownloadOutcome::Ok) {
        return WebSetupResult::Ok;
    }
    if (outcome == DownloadOutcome::Cancelled) {
        return WebSetupResult::Cancelled;
    }
    return failure;
}

/// Progress callback of the package download: the bar runs from 5 to 90 percent.
/// @param context A PackageProgress.
/// @param done Bytes received.
/// @param total The announced size (not used; the manifest size is trusted after its check).
void OnPackageProgress(void* context, std::uint64_t done, std::uint64_t total) {
    (void)total;
    PackageProgress* progress = static_cast<PackageProgress*>(context);
    int percent = 5 + static_cast<int>(done * 85 / progress->expected);
    if (percent == progress->lastPercent) {
        return;
    }
    progress->lastPercent = percent;
    std::wstring line = L"Downloading... " + std::to_wstring(done * 100 / progress->expected) + L" %";
    progress->host->Report(line.c_str(), percent);
}

/// Downloads and checks the manifest.
/// @param host The progress display and cancel flag.
/// @param manifest Receives the checked manifest.
/// @return Ok, Cancelled, ManifestDownload or ManifestInvalid.
WebSetupResult FetchManifest(SetupHost& host, WebManifest& manifest) {
    host.Report(L"Looking for the latest version...", 1);
    std::string body;
    DownloadRequest request;
    request.url = kManifestUrl;
    request.maxBytes = kManifestMaxBytes;
    request.allowRedirects = false;
    request.memory = &body;
    request.cancel = &host.CancelFlag();
    WebSetupResult downloaded = MapOutcome(Download(request), WebSetupResult::ManifestDownload);
    if (downloaded != WebSetupResult::Ok) {
        return downloaded;
    }
    ews::Reading<WebManifest> parsed = ews::ParseWebManifest(body);
    if (!parsed.ok()) {
        return WebSetupResult::ManifestInvalid;
    }
    manifest = parsed.value;
    return WebSetupResult::Ok;
}

/// @return 16 random hex digits from the system generator, or an empty text when it fails.
std::wstring RandomSuffix() {
    unsigned char bytes[8] = {};
    if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return std::wstring();
    }
    static const wchar_t digits[] = L"0123456789abcdef";
    std::wstring text;
    for (unsigned char byte : bytes) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 0x0F]);
    }
    return text;
}

/// Creates %TEMP%\EcoWattSense-Setup-<random>.
/// @param folder Receives the folder path.
/// @return Ok or WorkFolder.
WebSetupResult MakeWorkFolder(std::wstring& folder) {
    wchar_t temp[MAX_PATH + 2] = {};
    DWORD length = GetTempPathW(MAX_PATH + 1, temp);
    std::wstring suffix = RandomSuffix();
    if (length == 0 || length > MAX_PATH || suffix.empty()) {
        return WebSetupResult::WorkFolder;
    }
    std::wstring path = std::wstring(temp, length) + L"EcoWattSense-Setup-" + suffix;
    if (!CreateDirectoryW(path.c_str(), nullptr)) {
        return WebSetupResult::WorkFolder;
    }
    folder = path;
    return WebSetupResult::Ok;
}

/// Downloads the package into a new file. The file is closed when this function returns.
/// @param host The progress display and cancel flag.
/// @param manifest The checked manifest.
/// @param zipPath The new file.
/// @param received Receives the number of bytes that arrived.
/// @return Ok, Cancelled or PackageDownload.
WebSetupResult DownloadPackage(SetupHost& host, const WebManifest& manifest, const std::wstring& zipPath, std::uint64_t& received) {
    ews::UniqueHandle file(CreateFileW(zipPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) {
        return WebSetupResult::PackageDownload;
    }
    PackageProgress progress;
    progress.host = &host;
    progress.expected = manifest.size;
    DownloadRequest request;
    request.url.assign(manifest.zipUrl.begin(), manifest.zipUrl.end());
    request.maxBytes = manifest.size;
    request.allowRedirects = true;
    request.file = file.get();
    request.cancel = &host.CancelFlag();
    request.progress = OnPackageProgress;
    request.context = &progress;
    WebSetupResult result = MapOutcome(Download(request), WebSetupResult::PackageDownload);
    received = request.received;
    return result;
}

/// Downloads the package and checks its size against the manifest.
/// @param host The progress display and cancel flag.
/// @param manifest The checked manifest.
/// @param zipPath The new file.
/// @return Ok, Cancelled, PackageDownload or SizeMismatch.
WebSetupResult FetchPackage(SetupHost& host, const WebManifest& manifest, const std::wstring& zipPath) {
    host.Report(L"Downloading... 0 %", 5);
    std::uint64_t received = 0;
    WebSetupResult downloaded = DownloadPackage(host, manifest, zipPath, received);
    if (downloaded != WebSetupResult::Ok) {
        return downloaded;
    }
    if (!ews::SizeMatches(received, manifest.size)) {
        return WebSetupResult::SizeMismatch;
    }
    return WebSetupResult::Ok;
}

/// Checks the SHA256 of the downloaded file.
/// @param host The progress display.
/// @param manifest The checked manifest.
/// @param zipPath The package file.
/// @return Ok, HashFailed or HashMismatch.
WebSetupResult VerifyPackage(SetupHost& host, const WebManifest& manifest, const std::wstring& zipPath) {
    host.Report(L"Checking the download...", 90);
    ews::Reading<std::string> hash = ews::Sha256HexOfFile(zipPath.c_str());
    if (!hash.ok()) {
        return WebSetupResult::HashFailed;
    }
    if (!ews::Sha256Matches(hash.value, manifest.sha256)) {
        return WebSetupResult::HashMismatch;
    }
    return WebSetupResult::Ok;
}

/// Deletes the zip and the work folder (the folder only when it is empty) after a failed step.
/// @param zipPath The package file.
/// @param folder The work folder.
void DiscardDownload(const std::wstring& zipPath, const std::wstring& folder) {
    DeleteFileW(zipPath.c_str());
    RemoveDirectoryW(folder.c_str());
}

/// Runs a command line and waits for it.
/// @param application The program path.
/// @param commandLine The full command line.
/// @param workDirectory The current folder of the new process.
/// @return True when the program started, ended within the time limit and returned exit code 0.
bool RunAndWait(const std::wstring& application, std::wstring commandLine, const std::wstring& workDirectory) {
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info = {};
    BOOL created = CreateProcessW(application.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, workDirectory.c_str(), &startup, &info);
    if (!created) {
        return false;
    }
    ews::UniqueHandle process(info.hProcess);
    ews::UniqueHandle thread(info.hThread);
    if (WaitForSingleObject(process.get(), kExtractTimeoutMs) != WAIT_OBJECT_0) {
        TerminateProcess(process.get(), 1);
        return false;
    }
    DWORD exitCode = 1;
    GetExitCodeProcess(process.get(), &exitCode);
    return exitCode == 0;
}

/// Unpacks the zip into the work folder with %SystemRoot%\System32\tar.exe. This step is not cancellable; it takes seconds.
/// @param host The progress display and cancel flag.
/// @param zipPath The package file.
/// @param folder The work folder.
/// @return Ok, Cancelled or Extract.
WebSetupResult Extract(SetupHost& host, const std::wstring& zipPath, const std::wstring& folder) {
    if (host.CancelFlag().load()) {
        return WebSetupResult::Cancelled;
    }
    host.Report(L"Unpacking...", 95);
    wchar_t system[MAX_PATH + 2] = {};
    UINT length = GetSystemDirectoryW(system, MAX_PATH + 1);
    if (length == 0 || length > MAX_PATH) {
        return WebSetupResult::Extract;
    }
    std::wstring tar = std::wstring(system, length) + L"\\tar.exe";
    std::wstring commandLine = L"\"" + tar + L"\" -xf \"" + zipPath + L"\" -C \"" + folder + L"\"";
    if (!RunAndWait(tar, commandLine, folder)) {
        return WebSetupResult::Extract;
    }
    return WebSetupResult::Ok;
}

/// Starts <folder>\EcoWattSense\Setup.exe with ShellExecuteEx; Setup.exe asks for UAC itself.
/// @param host The progress display and cancel flag.
/// @param folder The work folder.
/// @param serverUrl The checked server address from the manifest, or empty; when set, Setup.exe gets /server=<url>.
/// @return Ok, Cancelled, SetupMissing or NotStarted (also when the UAC prompt was cancelled).
WebSetupResult LaunchSetup(SetupHost& host, const std::wstring& folder, const std::string& serverUrl) {
    if (host.CancelFlag().load()) {
        return WebSetupResult::Cancelled;
    }
    std::wstring directory = folder + L"\\EcoWattSense";
    std::wstring setup = directory + L"\\Setup.exe";
    DWORD attributes = GetFileAttributesW(setup.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return WebSetupResult::SetupMissing;
    }
    host.Report(L"Starting setup...", 99);
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.lpFile = setup.c_str();
    std::wstring arguments = L"/server=" + std::wstring(serverUrl.begin(), serverUrl.end());
    info.lpParameters = serverUrl.empty() ? nullptr : arguments.c_str();
    info.lpDirectory = directory.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        return WebSetupResult::NotStarted;
    }
    return WebSetupResult::Ok;
}

}  // namespace

WebSetupResult RunWebSetup(SetupHost& host) {
    WebManifest manifest;
    WebSetupResult result = FetchManifest(host, manifest);
    if (result != WebSetupResult::Ok) {
        return result;
    }
    std::wstring folder;
    result = MakeWorkFolder(folder);
    if (result != WebSetupResult::Ok) {
        return result;
    }
    std::wstring zipPath = folder + L"\\EcoWattSense.zip";
    result = FetchPackage(host, manifest, zipPath);
    if (result == WebSetupResult::Ok) {
        result = VerifyPackage(host, manifest, zipPath);
    }
    if (result != WebSetupResult::Ok) {
        DiscardDownload(zipPath, folder);
        return result;
    }
    result = Extract(host, zipPath, folder);
    if (result != WebSetupResult::Ok) {
        return result;
    }
    return LaunchSetup(host, folder, manifest.serverUrl);
}

}  // namespace websetup
