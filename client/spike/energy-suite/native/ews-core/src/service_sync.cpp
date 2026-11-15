// service_sync.cpp: the server sync parts of ServiceCore (see ews/service_core.h and SPEC 5.3, 5.5, 5.6, 5.7).
// Used by ews-service and the tests. It pings the server on the existing 60 second tick, reads the settings the admin
// changed, runs the commands of the ping answer and registers the computer again when the server lost it.
// Rules kept here: no timer of its own, every value from the server is range-checked first (server_sync.h), a failed
// call keeps the last good settings and never stops measuring or reporting, the log holds fixed words and numbers only.
#include <algorithm>
#include <filesystem>

#include "ews/atomic_file.h"
#include "ews/file_io.h"
#include "ews/report_queue.h"
#include "ews/service_core.h"

namespace ews {

namespace {

constexpr int kCounterCap = 1 << 20;                      ///< The most the tick counter of the ping grows to.
constexpr std::int64_t kRegisterWindowSeconds = 3600;     ///< The least time between two registrations while the server says false.
constexpr std::size_t kMaxNameFileBytes = 1024;           ///< The largest accepted device-name.txt.
constexpr std::size_t kMaxNameSyncFileBytes = 256;        ///< The largest accepted name-sync.json.

/// @param word The sorted answer of a ping.
/// @return The word of status.json.
LastPing LastPingFor(ApiWord word) {
    switch (word) {
        case ApiWord::Ok: return LastPing::Ok;
        case ApiWord::Unauthorised: return LastPing::Unauthorised;
        case ApiWord::RateLimited: return LastPing::RateLimited;
        case ApiWord::ServerError: return LastPing::ServerError;
        case ApiWord::Network: return LastPing::Network;
        case ApiWord::Rejected: return LastPing::Rejected;
    }
    return LastPing::Network;
}

/// @param type The type text of a command.
/// @return 1 set_interval, 2 report_now, 3 flush_buffer, 4 clear_buffer, 5 pull_settings, 0 any other text.
int CommandTypeCode(const std::string& type) {
    const char* const known[] = {"set_interval", "report_now", "flush_buffer", "clear_buffer", "pull_settings"};
    int code = 0;
    for (int index = 0; index < 5; ++index) {
        code = type == known[index] ? index + 1 : code;
    }
    return code;
}

/// @param result A command result.
/// @return 0 done, 1 failed, 2 unsupported (the number in the log).
int CommandResultCode(CommandResult result) { return static_cast<int>(result); }

/// @param dataDir The data folder.
/// @param name The file name below `<data>\status`.
/// @return The path of the file.
std::filesystem::path StatusFile(const std::wstring& dataDir, const wchar_t* name) {
    return std::filesystem::path(dataDir) / L"status" / name;
}

/// Writes a small file below `<data>\status` through a temporary file (the service cannot write to `config`).
/// @param dataDir The data folder.
/// @param name The file name.
/// @param text The content.
/// @return True when the file now holds the content.
bool WriteStatusFile(const std::wstring& dataDir, const wchar_t* name, const std::string& text) {
    Reading<bool> folder = EnsureSubfolder(dataDir, L"status");
    std::wstring statusDir = (std::filesystem::path(dataDir) / L"status").wstring();
    return folder.ok() && WriteFileAtomically(statusDir, name, text).ok();
}

/// @param config The settings in force.
/// @param pingIntervalSeconds The ping interval in force.
/// @return The settings in force as a ServerSettings without a version (used when a command changes them before any read).
ServerSettings SettingsInForce(const ServiceConfig& config, int pingIntervalSeconds) {
    ServerSettings settings;
    settings.reportIntervalSeconds = config.intervalSeconds;
    settings.pingIntervalSeconds = pingIntervalSeconds;
    settings.idleThresholdSeconds = config.idleThresholdSeconds;
    settings.unusedThresholdSeconds = config.unusedThresholdSeconds;
    return settings;
}

/// Writes `<folder>\device-name.txt` (one UTF-8 line) through a temporary file.
/// @param folder The Client folder.
/// @param name The checked name.
/// @return True when the file now holds the name.
bool WriteNameFile(const std::filesystem::path& folder, const std::string& name) {
    return WriteFileAtomically(folder.wstring(), L"device-name.txt", name + "\n").ok();
}

/// Deletes `<folder>\device-name.txt`.
/// @param folder The Client folder.
/// @return True when the file is gone (a missing file is not an error).
bool DeleteNameFile(const std::filesystem::path& folder) {
    std::error_code error;
    std::filesystem::remove(folder / L"device-name.txt", error);
    return !error;
}

}  // namespace

/// Reads commands-done.json and server-settings.json at start. A missing file is normal; a bad settings file is ignored and counted.
void ServiceCore::LoadSyncFiles() {
    Reading<std::string> nameSync = ReadWholeFile(StatusFile(settings_.dataDir, L"name-sync.json"), kMaxNameSyncFileBytes);
    nameSync_ = nameSync.ok() ? ParseNameSync(nameSync.value) : NameSync::None;
    Reading<std::string> done = ReadWholeFile(StatusFile(settings_.dataDir, L"commands-done.json"), kMaxCommandsFileBytes);
    doneCommands_ = done.ok() ? ParseCommandsDone(done.value) : std::vector<DoneCommand>{};
    Reading<std::string> text = ReadWholeFile(StatusFile(settings_.dataDir, L"server-settings.json"), kMaxSettingsFileBytes);
    // A missing file is "no server settings yet", not an error.
    if (text.status == Status::Unavailable) {
        return;
    }
    Reading<ServerSettings> parsed = text.ok() ? ParseServerSettings(text.value, false) : Reading<ServerSettings>::Failure(text.status, text.error);
    if (!parsed.ok()) {
        ++settingsIgnored_;
        Log(LogEvent::SettingsIgnored, {settingsIgnored_});
        return;
    }
    ApplySettings(parsed.value);
}

/// Reads the settings from the server once at start (when a server is set) and writes status.json. A failure keeps the local settings.
void ServiceCore::PullSettingsAtStart() {
    if (!api_) {
        return;
    }
    PullSettings();
    WriteStatus();
}

/// Puts settings in force at once: report interval, thresholds and ping interval. Takes the settings; returns nothing; cannot fail.
void ServiceCore::ApplySettings(const ServerSettings& settings) {
    config_.intervalSeconds = settings.reportIntervalSeconds;
    config_.idleThresholdSeconds = settings.idleThresholdSeconds;
    config_.unusedThresholdSeconds = settings.unusedThresholdSeconds;
    pingIntervalSeconds_ = settings.pingIntervalSeconds;
    serverSettings_ = settings;
    lastSettingsUnix_ = settings.checkedUnix;
    ignoredVersion_.reset();
}

/// Writes the settings in force to server-settings.json. A write failure is logged (state_write_failed 1); the settings stay in force.
void ServiceCore::SaveSettingsFile() {
    bool written = serverSettings_.has_value() && WriteStatusFile(settings_.dataDir, L"server-settings.json", FormatServerSettings(*serverSettings_));
    if (!written) {
        Log(LogEvent::StateWriteFailed, {1});
    }
}

/// Writes commands-done.json when it changed. A write failure is logged (state_write_failed 2).
void ServiceCore::SaveDoneCommands() {
    if (!doneDirty_) {
        return;
    }
    doneDirty_ = false;
    if (!WriteStatusFile(settings_.dataDir, L"commands-done.json", FormatCommandsDone(doneCommands_))) {
        Log(LogEvent::StateWriteFailed, {2});
    }
}

/// Calls GET /api/settings and applies a good answer. Returns Applied, Ignored (a bad or missing value; old settings stay) or Failed (no good HTTP answer).
ServiceCore::PullResult ServiceCore::PullSettings() {
    ApiAnswer answer = api_->Call("GET", "/api/settings", "");
    ApiWord word = ApiWordFor(answer.status);
    if (word != ApiWord::Ok) {
        Hold(answer);
        Log(LogEvent::SettingsFailed, {ApiWordCode(word)});
        return PullResult::Failed;
    }
    Reading<ServerSettings> parsed = ParseServerSettings(answer.body, true);
    if (!parsed.ok()) {
        ++settingsIgnored_;
        Log(LogEvent::SettingsIgnored, {settingsIgnored_});
        return PullResult::Ignored;
    }
    parsed.value.checkedUnix = NowUnix();
    ApplySettings(parsed.value);
    SaveSettingsFile();
    Log(LogEvent::SettingsApplied, {parsed.value.version.value_or(0)});
    return PullResult::Applied;
}

/// Keeps the next ping back for the Retry-After of a 429 or 503 answer (at most one hour). Takes the answer; returns nothing.
void ServiceCore::Hold(const ApiAnswer& answer) {
    int ticks = (HoldSecondsFor(answer) + settings_.tickSeconds - 1) / settings_.tickSeconds;
    pingHold_ = std::max(pingHold_, ticks);
}

/// Returns the number of ticks between two pings (at least 1).
int ServiceCore::PingTicks() const { return std::max(1, pingIntervalSeconds_ / 60); }

/// Returns the number of reports in the queue; keeps the last known number when the queue cannot be read.
int ServiceCore::UnsentCount() {
    Reading<int> queued = CountReports(settings_.dataDir);
    lastQueued_ = queued.ok() ? static_cast<std::uint32_t>(queued.value) : lastQueued_;
    return static_cast<int>(lastQueued_);
}

/// Counts one tick and pings when the ping interval has passed and no Retry-After hold runs. Does nothing without a server.
void ServiceCore::PingTick() {
    ticksSincePing_ = std::min(ticksSincePing_ + 1, kCounterCap);
    pingHold_ = std::max(0, pingHold_ - 1);
    bool due = api_ && ticksSincePing_ >= PingTicks() && pingHold_ == 0;
    if (due) {
        Ping();
    }
}

/// Sends POST /api/ping with the queue count and the acks, then handles the answer and writes status.json. A failure keeps all settings.
void ServiceCore::Ping() {
    ticksSincePing_ = 0;
    std::vector<DoneCommand> sentAcks = doneCommands_;
    ApiAnswer answer = api_->Call("POST", "/api/ping", BuildPingBody(deviceId_, UnsentCount(), sentAcks));
    ApiWord word = ApiWordFor(answer.status);
    Reading<PingAnswer> parsed = word == ApiWord::Ok ? ParsePingAnswer(answer.body) : Reading<PingAnswer>();
    // A 200 whose body cannot be understood counts as a server error: nothing in it is trusted.
    word = (word == ApiWord::Ok && !parsed.ok()) ? ApiWord::ServerError : word;
    NotePing(word, answer);
    if (word == ApiWord::Ok) {
        HandlePing(parsed.value, sentAcks);
    }
    WriteStatus();
}

/// Records the result of a ping (word, time, failures in a row) and logs the first failure and then one an hour.
void ServiceCore::NotePing(ApiWord word, const ApiAnswer& answer) {
    lastPing_ = LastPingFor(word);
    lastPingUnix_ = NowUnix();
    pingFailures_ = word == ApiWord::Ok ? 0 : pingFailures_ + 1;
    Hold(answer);
    // One line for the first failure and then one an hour (at 60 seconds), so a long outage cannot fill the log.
    bool logIt = word != ApiWord::Ok && (pingFailures_ == 1 || pingFailures_ % 60 == 0);
    if (logIt) {
        Log(LogEvent::PingFailed, {ApiWordCode(word), pingFailures_});
    }
}

/// Acts on a good ping answer: reads new settings, runs commands, drops the acks that got 200 and registers the computer when asked. Takes the answer and the acks the ping carried.
void ServiceCore::HandlePing(const PingAnswer& answer, const std::vector<DoneCommand>& sentAcks) {
    CheckSettingsVersion(answer.settingsVersion);
    RunCommands(answer.commands);
    // The ping that carried the acks got 200: they are done with, unless the server lists the command again.
    doneCommands_ = WithoutAcked(doneCommands_, sentAcks, answer.commands);
    doneDirty_ = doneDirty_ || !sentAcks.empty();
    SaveDoneCommands();
    bool putSent = !answer.registered && RegisterDevice();
    RetryNameSync(putSent);
}

/// Reads the settings when the version of the ping answer is not the one in force and not one whose answer was bad.
void ServiceCore::CheckSettingsVersion(std::int64_t version) {
    bool inForce = serverSettings_.has_value() && serverSettings_->version == version;
    bool skipped = ignoredVersion_ == version;
    if (inForce || skipped) {
        return;
    }
    PullResult pulled = PullSettings();
    // A bad answer is not asked for again until the server shows another version (or a pull_settings command comes).
    ignoredVersion_ = pulled == PullResult::Ignored ? std::optional<std::int64_t>(version) : ignoredVersion_;
}

/// Runs the commands of a ping answer in the order given (id order).
void ServiceCore::RunCommands(const std::vector<ServerCommand>& commands) {
    for (const ServerCommand& command : commands) {
        RunOneCommand(command);
    }
}

/// Runs one command unless it is expired or its id is already handled; remembers its result for the next ack and logs it.
void ServiceCore::RunOneCommand(const ServerCommand& command) {
    bool expired = command.expiresUnix < NowUnix();
    if (expired || HasDone(doneCommands_, command.id)) {
        return;
    }
    CommandResult result = Execute(command);
    doneCommands_.push_back(DoneCommand{command.id, result});
    std::size_t extra = doneCommands_.size() > kMaxDoneCommands ? doneCommands_.size() - kMaxDoneCommands : 0;
    doneCommands_.erase(doneCommands_.begin(), doneCommands_.begin() + static_cast<std::ptrdiff_t>(extra));
    doneDirty_ = true;
    Log(LogEvent::CommandRun, {command.id, CommandTypeCode(command.type), CommandResultCode(result)});
}

/// Runs the command of a known type. Returns done or failed; unsupported for any other type.
CommandResult ServiceCore::Execute(const ServerCommand& command) {
    switch (CommandTypeCode(command.type)) {
        case 1: return CommandSetInterval(command);
        case 2: return CommandReportNow();
        case 3: return CommandFlushBuffer();
        case 4: return CommandClearBuffer();
        case 5: return CommandPullSettings();
        default: return CommandResult::Unsupported;
    }
}

/// set_interval: puts a new report interval in force and stores it. Returns failed when the value is missing or not in 60 to 3600.
CommandResult ServiceCore::CommandSetInterval(const ServerCommand& command) {
    std::optional<int> seconds = command.intervalParam.has_value() ? NormaliseReportInterval(*command.intervalParam) : std::nullopt;
    if (!seconds.has_value()) {
        return CommandResult::Failed;
    }
    ServerSettings updated = serverSettings_.value_or(SettingsInForce(config_, pingIntervalSeconds_));
    updated.reportIntervalSeconds = *seconds;
    ApplySettings(updated);
    SaveSettingsFile();
    return CommandResult::Done;
}

/// report_now: closes the running report period, queues and sends its report, and starts a new period. Returns done.
CommandResult ServiceCore::CommandReportNow() {
    Snapshot at = CloseEarly(true);
    BeginInterval(at);
    return CommandResult::Done;
}

/// flush_buffer: tries a send of the queue now (the path of the pipe request). Returns done, also when there was nothing to send.
CommandResult ServiceCore::CommandFlushBuffer() {
    SendNow(LogEvent::SendRequested);
    return CommandResult::Done;
}

/// clear_buffer: empties the queue file and logs how many reports it held. Returns failed when the file cannot be written.
CommandResult ServiceCore::CommandClearBuffer() {
    Reading<int> queued = CountReports(settings_.dataDir);
    Reading<bool> folder = EnsureSubfolder(settings_.dataDir, L"queue");
    std::wstring queueDir = (std::filesystem::path(settings_.dataDir) / L"queue").wstring();
    // An empty file is an empty queue: sent and unsent lines are gone, and so is any damaged line.
    bool cleared = folder.ok() && WriteFileAtomically(queueDir, L"reports.jsonl", "").ok();
    if (!cleared) {
        return CommandResult::Failed;
    }
    Log(LogEvent::BufferCleared, {queued.ok() ? queued.value : 0});
    return CommandResult::Done;
}

/// pull_settings: reads the settings now. Returns done when a good answer was applied, else failed.
CommandResult ServiceCore::CommandPullSettings() {
    return PullSettings() == PullResult::Applied ? CommandResult::Done : CommandResult::Failed;
}

/// Sends PUT /api/devices/{device_id} with the computer name, at most once an hour. Returns true when it was sent.
bool ServiceCore::RegisterDevice() {
    std::int64_t now = NowUnix();
    bool recent = lastRegisterUnix_.has_value() && now >= *lastRegisterUnix_ && now - *lastRegisterUnix_ < kRegisterWindowSeconds;
    if (recent) {
        return false;
    }
    lastRegisterUnix_ = now;
    PutName();
    return true;
}

/// Returns the computer name of device-name.txt, or nothing when the file is missing, empty or bad.
std::optional<std::string> ServiceCore::ReadLocalName() {
    std::filesystem::path nameFile = std::filesystem::path(settings_.dataDir) / L"Client" / L"device-name.txt";
    Reading<std::string> text = ReadWholeFile(nameFile, kMaxNameFileBytes);
    return text.ok() ? DeviceNameFromFile(text.value) : std::nullopt;
}

/// Writes the name to device-name.txt through a temporary file, or deletes the file for no name. Returns true when it worked.
bool ServiceCore::StoreLocalName(const std::optional<std::string>& name) {
    std::filesystem::path folder = std::filesystem::path(settings_.dataDir) / L"Client";
    return name.has_value() ? WriteNameFile(folder, *name) : DeleteNameFile(folder);
}

/// Handles the pipe message `EWS1 name=<base64>`: a bad message, or one less than 10 seconds after the last good one, is dropped and counted.
void ServiceCore::NameRequested(std::string_view text) {
    Reading<std::optional<std::string>> parsed = ParseNameMessage(text);
    std::int64_t now = NowUnix();
    bool early = lastNameRequestUnix_.has_value() && now >= *lastNameRequestUnix_ && now - *lastNameRequestUnix_ < kNameMessageGapSeconds;
    if (!parsed.ok() || early) {
        ++droppedPipeMessages_;
        return;
    }
    lastNameRequestUnix_ = now;
    if (!StoreLocalName(parsed.value)) {
        Log(LogEvent::StateWriteFailed, {3});
        return;
    }
    SetNameSync(NameSync::Pending);
    // While a Retry-After hold runs the PUT waits; the ping tick sends it later.
    bool sendNow = api_ && pingHold_ == 0;
    if (sendNow) {
        PutName();
    }
    WriteStatus();
}

/// Sends PUT /api/devices/{device_id} with the local name and the device type, then notes the answer.
void ServiceCore::PutName() {
    ApiAnswer answer = api_->Call("PUT", "/api/devices/" + deviceId_, BuildDeviceBody(ReadLocalName(), config_.deviceType));
    Hold(answer);
    NoteNameAnswer(answer);
}

/// Sends the PUT again when the name is pending and neither a PUT of this ping nor a Retry-After hold stands in the way.
void ServiceCore::RetryNameSync(bool putSent) {
    bool again = !putSent && nameSync_ == NameSync::Pending && pingHold_ == 0;
    if (again) {
        PutName();
    }
}

/// Notes the answer of a PUT. 200 is ok and keeps the name the server holds; 401 and other 4xx (422) are failed and not
/// tried again; anything else (network, 429, 5xx) leaves the name pending.
void ServiceCore::NoteNameAnswer(const ApiAnswer& answer) {
    ApiWord word = ApiWordFor(answer.status);
    Reading<DeviceAnswer> parsed = word == ApiWord::Ok ? ParseDeviceAnswer(answer.body) : Reading<DeviceAnswer>();
    serverNameKnown_ = serverNameKnown_ || parsed.ok();
    serverName_ = parsed.ok() ? parsed.value.name : serverName_;
    bool refused = word == ApiWord::Unauthorised || word == ApiWord::Rejected;
    NameSync state = refused ? NameSync::Failed : NameSync::Pending;
    SetNameSync(word == ApiWord::Ok ? NameSync::Ok : state);
    Log(LogEvent::DeviceRegistered, {ApiWordCode(word)});
}

/// Remembers the name state; writes name-sync.json below the status folder when the state changed.
void ServiceCore::SetNameSync(NameSync state) {
    bool changed = state != nameSync_;
    nameSync_ = state;
    if (changed && !WriteStatusFile(settings_.dataDir, L"name-sync.json", FormatNameSync(state))) {
        Log(LogEvent::StateWriteFailed, {3});
    }
}

}  // namespace ews
