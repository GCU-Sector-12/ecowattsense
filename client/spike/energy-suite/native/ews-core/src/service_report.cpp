// service_report.cpp: the report, queue, send, model reload and status parts of ServiceCore (see ews/service_core.h).
// Used by ews-service and the tests. The lifecycle and the minute calculation are in service_core.cpp.
// The log gets fixed words and whole numbers only: no URL, no key, no file content, no server text.
#include <windows.h>

#include <filesystem>

#include "ews/atomic_file.h"
#include "ews/backoff.h"
#include "ews/file_io.h"
#include "ews/report_json.h"
#include "ews/report_queue.h"
#include "ews/service_core.h"

namespace ews {

namespace {

/// The most full batches sent in one report tick, so a long queue cannot hold the loop for long.
constexpr int kMaxBatchesPerTick = 5;

/// The shortest time between two sends caused by the pipe command `EWS1 send`.
constexpr std::int64_t kSendRequestWindowSeconds = 30;

/// The largest accepted model file.
constexpr std::size_t kMaxModelBytes = 64 * 1024;

/// @return The text of status.json level `n`: "L1" to "L4".
std::string LevelText(int number) { return "L" + std::to_string(number); }

/// @return The status word that belongs to a send word.
LastSend LastSendFor(SendWord word) {
    switch (word) {
        case SendWord::Ok: return LastSend::Ok;
        case SendWord::Unauthorised: return LastSend::Unauthorised;
        case SendWord::Rejected: return LastSend::Rejected;
        case SendWord::ServerError: return LastSend::ServerError;
        case SendWord::Network: return LastSend::Network;
    }
    return LastSend::Network;
}

/// @return The log event that belongs to a send word.
LogEvent EventForSend(SendWord word) {
    switch (word) {
        case SendWord::Ok: return LogEvent::SendOk;
        case SendWord::Unauthorised: return LogEvent::SendUnauthorised;
        case SendWord::Rejected: return LogEvent::SendRejected;
        case SendWord::ServerError: return LogEvent::SendServerError;
        case SendWord::Network: return LogEvent::SendNetwork;
    }
    return LogEvent::SendNetwork;
}

/// @return True when the answer means "try again later" (the queue stays and the back-off grows).
bool IsRetryLater(SendWord word) {
    return word == SendWord::Unauthorised || word == SendWord::ServerError || word == SendWord::Network;
}

/// @return The last write time of a file as one number, or nothing when the file is missing.
std::optional<std::int64_t> FileWriteTime(const std::filesystem::path& file) {
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &info)) {
        return std::nullopt;
    }
    ULARGE_INTEGER stamp{};
    stamp.LowPart = info.ftLastWriteTime.dwLowDateTime;
    stamp.HighPart = info.ftLastWriteTime.dwHighDateTime;
    return static_cast<std::int64_t>(stamp.QuadPart);
}

/// @return The path of calibration\model.json.
std::filesystem::path ModelPath(const std::wstring& dataDir) { return std::filesystem::path(dataDir) / L"calibration" / L"model.json"; }

}  // namespace

void ServiceCore::FinishInterval(const Snapshot& at, bool trySend) {
    ReleaseHeldRowsAtIntervalEnd();
    if (!interval_.has_value()) {
        return;
    }
    IntervalResult result = interval_->Close(ToUnixSeconds(at.time), at.battery);
    interval_.reset();
    if (result.dropped) {
        return;
    }
    QueueReport(result);
    FlushCounters();
    if (trySend) {
        SendQueued();
    }
    ReloadModelIfChanged();
    WriteStatus();
}

ReportFields ServiceCore::MakeFields(const IntervalResult& result) {
    ReportFields fields;
    fields.deviceId = deviceId_;
    fields.timestampUnix = result.endUnix;
    fields.intervalSeconds = result.intervalSeconds;
    fields.idleSeconds = ComputeIdleSeconds(activity_, result.endUnix, serviceStartUnix_);
    fields.state = ComputeState(fields.idleSeconds, config_.idleThresholdSeconds, config_.unusedThresholdSeconds, !somebodyLoggedOn_);
    fields.cpuPercent = result.cpuPercent;
    fields.screenLocked = !somebodyLoggedOn_ || activeSessionLocked_;
    fields.onBattery = result.onBattery;
    fields.deviceType = config_.deviceType;
    fields.energyUj = result.energyUj;
    fields.batteryEnergyUj = result.batteryEnergyUj;
    fields.deviceEnergyUj = result.deviceEnergyUj;
    return fields;
}

void ServiceCore::QueueReport(const IntervalResult& result) {
    // The server requires the processor percent: an interval without it gives no report, and it is counted.
    if (!result.cpuPercent.has_value()) {
        ++skippedReports_;
        Log(LogEvent::ReportSkipped, {skippedReports_});
        return;
    }
    Reading<std::string> json = BuildReportJson(MakeFields(result));
    if (!json.ok()) {
        Log(LogEvent::CountedError, {1});
        return;
    }
    QueueAppendResult queued = AppendReport(settings_.dataDir, json.value);
    LogQueueOutcome(queued);
    lastReportUnix_ = queued.outcome == QueueAppend::Added ? std::optional<std::int64_t>(result.endUnix) : lastReportUnix_;
}

void ServiceCore::LogQueueOutcome(const QueueAppendResult& queued) {
    if (queued.outcome != QueueAppend::Added) {
        Log(LogEvent::QueueWriteFailed);
    }
    if (queued.repairs.droppedOldest > 0) {
        Log(LogEvent::QueueDropped, {queued.repairs.droppedOldest});
    }
    if (queued.repairs.skippedLines > 0) {
        Log(LogEvent::QueueSkipped, {queued.repairs.skippedLines});
    }
    if (queued.repairs.brokenFiles > 0) {
        Log(LogEvent::QueueBrokenFile);
    }
}

void ServiceCore::FlushCounters() {
    FlushCounter(LogEvent::PipeDropped, droppedPipeMessages_);
    FlushCounter(LogEvent::BatteryJump, batteryJumps_);
    FlushCounter(LogEvent::BatteryReplaced, batteryReplaced_);
}

void ServiceCore::FlushCounter(LogEvent event, int& count) {
    if (count <= 0) {
        return;
    }
    Log(event, {count});
    count = 0;
}

void ServiceCore::NoteDisabled() {
    lastSend_ = LastSend::Disabled;
    if (sendReasonLogged_) {
        return;
    }
    sendReasonLogged_ = true;
    // 1: sending is off or no URL is set. 2: the key is missing or cannot be opened.
    Log(LogEvent::SendDisabled, {config_.SendingOn() ? 2 : 1});
}

void ServiceCore::SendQueued() {
    if (!sender_) {
        NoteDisabled();
        return;
    }
    if (waitIntervals_ > 0) {
        --waitIntervals_;
        return;
    }
    int batches = 0;
    while (batches < kMaxBatchesPerTick && SendOneBatch()) {
        ++batches;
    }
}

bool ServiceCore::SendNow(LogEvent event) {
    // No server (sending off, no URL or no key) or nothing queued: nothing to try, and nothing is logged.
    Reading<int> queued = CountReports(settings_.dataDir);
    bool wanted = sender_ && queued.ok() && queued.value > 0;
    if (!wanted) {
        return false;
    }
    Log(event);
    sendFailures_ = 0;
    waitIntervals_ = 0;
    SendQueued();
    WriteStatus();
    return true;
}

void ServiceCore::SendRequested() {
    std::int64_t now = NowUnix();
    bool tooSoon = lastSendRequestUnix_.has_value() && now >= *lastSendRequestUnix_ && now - *lastSendRequestUnix_ < kSendRequestWindowSeconds;
    if (tooSoon) {
        ++droppedPipeMessages_;
        return;
    }
    bool tried = SendNow(LogEvent::SendRequested);
    lastSendRequestUnix_ = tried ? std::optional<std::int64_t>(now) : lastSendRequestUnix_;
}

bool ServiceCore::SendOneBatch() {
    Reading<QueueBatch> batch = PeekReports(settings_.dataDir, kMaxBatchLines);
    if (!batch.ok()) {
        Log(LogEvent::QueueWriteFailed);
        return false;
    }
    LogQueueOutcome(QueueAppendResult{QueueAppend::Added, batch.value.repairs});
    int lines = static_cast<int>(batch.value.lines.size());
    if (lines == 0) {
        return false;
    }
    SendResult result = sender_->Send(batch.value.lines);
    HandleSendResult(result, lines);
    return result.word == SendWord::Ok && result.leaveQueue && lines == kMaxBatchLines;
}

void ServiceCore::HandleSendResult(const SendResult& result, int lines) {
    lastSendUnix_ = NowUnix();
    lastSend_ = LastSendFor(result.word);
    bool removed = !result.leaveQueue || RemoveFirstReports(settings_.dataDir, lines).ok();
    if (!removed) {
        Log(LogEvent::QueueWriteFailed);
    }
    LogSend(result, lines);
    sendFailures_ = IsRetryLater(result.word) ? sendFailures_ + 1 : 0;
    waitIntervals_ = BackoffIntervals(sendFailures_, BackoffCapIntervals(config_.intervalSeconds));
}

void ServiceCore::LogSend(const SendResult& result, int lines) {
    LogEvent event = EventForSend(result.word);
    if (result.word == SendWord::Ok) {
        Log(event, {lines, result.rejected});
        return;
    }
    if (result.word == SendWord::Rejected) {
        Log(event, {lines});
        return;
    }
    Log(event);
}

void ServiceCore::ReloadModelIfChanged() {
    if (FileWriteTime(ModelPath(settings_.dataDir)) == modelFileTime_) {
        return;
    }
    LoadModel();
    CheckLevelChange();
}

void ServiceCore::LoadModel() {
    modelFileTime_ = FileWriteTime(ModelPath(settings_.dataDir));
    model_.reset();
    Reading<std::string> text = ReadWholeFile(ModelPath(settings_.dataDir), kMaxModelBytes);
    // A missing file is "no model yet", not an error.
    if (text.status == Status::Unavailable) {
        return;
    }
    Reading<CalibrationModel> parsed = text.ok() ? ParseModelJson(text.value) : Reading<CalibrationModel>::Failure(text.status, text.error);
    if (!parsed.ok()) {
        Log(LogEvent::ModelInvalid);
        return;
    }
    model_ = std::move(parsed.value);
}

int ServiceCore::EffectiveLevelNumber() const {
    bool needsModel = scanLevel_ == MeasurementLevel::L2 || scanLevel_ == MeasurementLevel::L3;
    if (needsModel && !model_.has_value()) {
        return 4;
    }
    return static_cast<int>(scanLevel_) + 1;
}

void ServiceCore::CheckLevelChange() {
    int now = EffectiveLevelNumber();
    if (now == loggedLevel_) {
        return;
    }
    Log(LogEvent::LevelChange, {loggedLevel_, now});
    loggedLevel_ = now;
}

void ServiceCore::WriteStatus() {
    StatusFields fields;
    fields.serviceVersion = kServiceVersion;
    fields.level = LevelText(EffectiveLevelNumber());
    fields.modelGeneratedUtc = model_.has_value() ? std::optional<std::string>(model_->generatedUtc) : std::nullopt;
    fields.lastReportUnix = lastReportUnix_;
    fields.lastSendUnix = lastSendUnix_;
    fields.lastSendResult = lastSend_;
    // A queue that cannot be counted keeps the last known number instead of showing 0.
    Reading<int> queued = CountReports(settings_.dataDir);
    lastQueued_ = queued.ok() ? static_cast<std::uint32_t>(queued.value) : lastQueued_;
    fields.queued = lastQueued_;
    fields.serverUrl = config_.serverUrl;
    fields.serverKeySet = keySet_;
    fields.serverEnabled = config_.serverEnabled;
    fields.deviceType = config_.deviceType;
    fields.intervalSeconds = config_.intervalSeconds;
    fields.idleThresholdSeconds = config_.idleThresholdSeconds;
    fields.unusedThresholdSeconds = config_.unusedThresholdSeconds;
    Reading<bool> folder = EnsureSubfolder(settings_.dataDir, L"status");
    std::wstring statusDir = (std::filesystem::path(settings_.dataDir) / L"status").wstring();
    bool written = folder.ok() && WriteFileAtomically(statusDir, L"status.json", BuildStatusJson(fields)).ok();
    if (!written) {
        Log(LogEvent::StatusWriteFailed);
    }
}

}  // namespace ews
