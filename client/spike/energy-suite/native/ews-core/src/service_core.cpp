// service_core.cpp: the lifecycle, the entry points and the minute calculation of ServiceCore (see ews/service_core.h).
// Used by ews-service and the tests. The report, queue, send and status parts are in service_report.cpp.
// Rules kept here: one thread, no waiting, an unknown value stays unknown (never 0), no text from outside in the log.
#include "ews/service_core.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>

#include "ews/atomic_file.h"
#include "ews/device_identity.h"
#include "ews/file_io.h"
#include "ews/history_file.h"
#include "ews/history_format.h"
#include "ews/scan_output.h"
#include "ews/secret.h"

namespace ews {

namespace {

/// A tick that comes after more than this many tick lengths (or with the clock gone back) is a gap.
constexpr int kGapTickFactor = 3;

/// @return The processor load between two snapshots, or nothing when either read failed or the counters are not usable.
std::optional<double> CpuBetween(const Snapshot& previous, const Snapshot& current) {
    if (!previous.cpu.has_value() || !current.cpu.has_value()) {
        return std::nullopt;
    }
    Reading<double> percent = CpuPercent(*previous.cpu, *current.cpu);
    return percent.ok() ? std::optional<double>(percent.value) : std::nullopt;
}

/// @return The minute as the interval accumulator needs it, with its battery change (empty when impossible or unknown).
IntervalMinute MakeIntervalMinute(double seconds, const std::optional<double>& cpuPercent, const MinuteEnergy& energy,
                                  const MinuteBattery& battery) {
    IntervalMinute minute;
    minute.seconds = seconds;
    minute.cpuPercent = cpuPercent;
    minute.deviceUj = energy.deviceUj;
    minute.batteryUj = battery.changeUj;
    minute.batteryJump = battery.jump;
    return minute;
}

/// @return Watts for an energy in uJ over some seconds, or nothing when the energy is unknown.
std::optional<double> ToWatts(const std::optional<double>& microjoules, double seconds) {
    if (!microjoules.has_value() || seconds <= 0.0) {
        return std::nullopt;
    }
    return *microjoules / seconds / 1.0e6;
}

}  // namespace

ServiceCore::ServiceCore(const ServiceDeps& deps, ServiceSettings settings) : deps_(deps), settings_(std::move(settings)) {}

StartResult ServiceCore::Start() {
    StartResult prepared = Prepare();
    if (prepared != StartResult::Ok) {
        return prepared;
    }
    LoadSetup();
    BeginRun();
    return StartResult::Ok;
}

StartResult ServiceCore::Prepare() {
    std::error_code ignored;
    if (!std::filesystem::is_directory(std::filesystem::path(settings_.dataDir), ignored)) {
        return StartResult::DataFolderMissing;
    }
    LockState lock = lock_.Acquire(settings_.dataDir);
    if (lock == LockState::Held) {
        Log(LogEvent::LockHeld);
        return StartResult::LockHeld;
    }
    if (lock == LockState::Failed) {
        Log(LogEvent::StartFailed, {2, static_cast<long long>(lock_.error())});
        return StartResult::LockFailed;
    }
    Reading<DeviceIdentity> identity = LoadOrCreateDeviceIdentity(settings_.dataDir);
    if (!identity.ok()) {
        lock_.Release();
        Log(LogEvent::StartFailed, {3, static_cast<long long>(identity.error)});
        return StartResult::IdentityFailed;
    }
    deviceId_ = identity.value.id;
    return StartResult::Ok;
}

void ServiceCore::LoadSetup() {
    ScanResult scan = deps_.scan.Run(ScanContext::Service);
    scanLevel_ = scan.level;
    displayReadable_ = scan.display.common.readable == Tri::True;
    WriteScanFiles(scan);
    config_ = LoadServiceConfig(settings_.dataDir).value;
    if (config_.badValues > 0) {
        Log(LogEvent::ConfigBadValues, {config_.badValues});
    }
    LoadSending();
    LoadModel();
}

void ServiceCore::BeginRun() {
    Reading<bool> power = deps_.power.OnBattery();
    onBattery_ = power.ok() ? power.value : false;
    Reading<std::vector<EmiChannel>> opened = deps_.emi.Open();
    channels_ = opened.ok() ? opened.value : std::vector<EmiChannel>{};
    Snapshot first = TakeSnapshot();
    serviceStartUnix_ = ToUnixSeconds(first.time);
    BeginInterval(first);
    CleanHistoryOnNewDay(first.time);
    running_ = true;
    Log(LogEvent::ServiceStart);
    CheckLevelChange();
    WriteStatus();
}

void ServiceCore::LoadSending() {
    sender_.reset();
    if (!config_.SendingOn()) {
        return;
    }
    Reading<std::string> key = LoadServerKey(settings_.dataDir);
    if (!key.ok()) {
        return;
    }
    sender_ = deps_.senders.Create(config_.serverUrl, key.value);
    SecureZeroMemory(key.value.data(), key.value.size());
}

void ServiceCore::WriteScanFiles(const ScanResult& scan) {
    Reading<bool> folder = EnsureSubfolder(settings_.dataDir, L"scan");
    std::wstring scanDir = (std::filesystem::path(settings_.dataDir) / L"scan").wstring();
    bool json = folder.ok() && WriteFileAtomically(scanDir, CapabilitiesFileName(scan.context), WriteCapabilitiesJson(scan)).ok();
    bool text = folder.ok() && WriteFileAtomically(scanDir, ScanLogFileName(scan.context), WriteScanLog(scan)).ok();
    if (!json || !text) {
        Log(LogEvent::ScanWriteFailed);
    }
}

Snapshot ServiceCore::TakeSnapshot() {
    Snapshot snapshot;
    snapshot.time = deps_.clock.Now();
    Reading<std::vector<EmiSample>> emi = deps_.emi.Read();
    snapshot.emiKnown = emi.ok() && emi.value.size() == channels_.size();
    snapshot.emi = emi.ok() ? std::move(emi.value) : std::vector<EmiSample>{};
    snapshot.battery = ToBatteryReading(deps_.battery.Read());
    Reading<CpuTimes> cpu = deps_.cpu.Read();
    snapshot.cpu = cpu.ok() ? std::optional<CpuTimes>(cpu.value) : std::nullopt;
    return snapshot;
}

void ServiceCore::BeginInterval(const Snapshot& at) {
    interval_.emplace(ToUnixSeconds(at.time), onBattery_, at.battery);
    ticksInInterval_ = 0;
    previous_ = at;
}

Snapshot ServiceCore::CloseEarly(bool trySend) {
    Snapshot now = TakeSnapshot();
    ProcessMinute(now);
    FinishInterval(now, trySend);
    return now;
}

std::optional<double> ServiceCore::ReadDisplayBrightness() {
    Reading<DisplayBrightness> levels = deps_.display.Read();
    if (!levels.ok()) {
        return std::nullopt;
    }
    Reading<int> percent = BrightnessPercent(levels.value, onBattery_);
    return percent.ok() ? std::optional<double>(percent.value) : std::nullopt;
}

std::optional<double> ServiceCore::CurrentBrightness() {
    std::optional<double> fromDisplay = displayReadable_ ? ReadDisplayBrightness() : std::nullopt;
    if (fromDisplay.has_value()) {
        return fromDisplay;
    }
    return pipeBrightness_.has_value() ? std::optional<double>(*pipeBrightness_) : std::nullopt;
}

void ServiceCore::ProcessMinute(const Snapshot& current) {
    double seconds = std::chrono::duration<double>(current.time - previous_.time).count();
    // A fraction of a second is left for the next minute: its energy is not lost, it is added there.
    if (!(seconds >= 1.0) || !interval_.has_value()) {
        return;
    }
    std::optional<double> cpuPercent = CpuBetween(previous_, current);
    EmiGrowth growth = ComputeEmiGrowth(channels_, previous_, current);
    MinuteInput input;
    input.seconds = seconds;
    input.cpuPercent = cpuPercent;
    input.systemGrowthUj = growth.systemUj;
    input.packageGrowthUj = growth.packageUj;
    input.dramGrowthUj = growth.dramUj;
    input.displayOn = displayOn_;
    input.brightnessPct = CurrentBrightness();
    input.model = model_.has_value() ? &*model_ : nullptr;
    MinuteEnergy energy = CalculateMinuteEnergy(input);
    MinuteBattery battery = JudgeBatteryChange(previous_.battery, current.battery, onBattery_, seconds);
    batteryJumps_ += battery.jump ? 1 : 0;
    interval_->AddMinute(MakeIntervalMinute(seconds, cpuPercent, energy, battery));
    AppendHistory(current, seconds, cpuPercent, energy, battery);
    previous_ = current;
}

void ServiceCore::AppendHistory(const Snapshot& current, double seconds, const std::optional<double>& cpuPercent,
                                const MinuteEnergy& energy, const MinuteBattery& battery) {
    HistoryRow row;
    row.timestampUnix = ToUnixSeconds(current.time);
    row.seconds = std::llround(seconds);
    row.onBattery = onBattery_;
    std::int64_t idle = ComputeIdleSeconds(activity_, row.timestampUnix, serviceStartUnix_);
    row.idleSeconds = idle;
    row.state = ComputeState(idle, config_.idleThresholdSeconds, config_.unusedThresholdSeconds, !somebodyLoggedOn_);
    row.cpuPct = cpuPercent;
    row.deviceW = ToWatts(energy.deviceUj, seconds);
    row.measuredW = energy.deviceUj.has_value() ? ToWatts(energy.measuredUj, seconds) : std::nullopt;
    row.batteryW = ToWatts(battery.changeUj, seconds);
    row.level = energy.level;
    RecordRow(row, current.time, battery, seconds);
}

void ServiceCore::RecordRow(const HistoryRow& row, UtcTime time, const MinuteBattery& battery, double seconds) {
    // An impossible minute waits for the first possible minute after it (SPEC 4.3).
    if (battery.jump) {
        heldRows_.push_back(HeldRow{row, time, true});
        return;
    }
    bool holding = !heldRows_.empty();
    // A minute with an unknown change does not resolve anything; it queues behind the held rows to keep the order.
    if (holding && !battery.changeUj.has_value()) {
        heldRows_.push_back(HeldRow{row, time, false});
        return;
    }
    if (holding) {
        interval_->ResolveBatteryJumps(*battery.changeUj / seconds);
    }
    WriteHeldRows(row.batteryW);
    WriteRow(row, time);
}

void ServiceCore::WriteHeldRows(const std::optional<double>& replacedWatts) {
    for (HeldRow& held : heldRows_) {
        bool replaced = held.replace && replacedWatts.has_value();
        batteryReplaced_ += replaced ? 1 : 0;
        held.row.batteryW = replaced ? replacedWatts : held.row.batteryW;
        WriteRow(held.row, held.time);
    }
    heldRows_.clear();
}

void ServiceCore::WriteRow(const HistoryRow& row, UtcTime time) {
    Reading<bool> written = AppendHistoryRow(settings_.dataDir, time, kHistoryHeader, FormatHistoryRow(row));
    // One line when the writing starts to fail, not one per minute.
    if (!written.ok() && !historyFailing_) {
        Log(LogEvent::HistoryWriteFailed, {static_cast<long long>(written.error)});
    }
    historyFailing_ = !written.ok();
}

void ServiceCore::CleanHistoryOnNewDay(UtcTime now) {
    std::string today = FormatUtcDate(now);
    if (today == lastDate_) {
        return;
    }
    lastDate_ = today;
    Reading<int> deleted = DeleteOldHistoryFiles(settings_.dataDir, now);
    if (deleted.ok() && deleted.value > 0) {
        Log(LogEvent::HistoryCleaned, {deleted.value});
    }
}

bool ServiceCore::IsGap(const Snapshot& current) const {
    double elapsed = std::chrono::duration<double>(current.time - previous_.time).count();
    return elapsed < 0.0 || elapsed > static_cast<double>(settings_.tickSeconds) * kGapTickFactor;
}

void ServiceCore::HandleGap(const Snapshot& current) {
    Log(LogEvent::GapDetected);
    FinishInterval(previous_, true);
    BeginInterval(current);
}

int ServiceCore::TicksPerInterval() const { return std::max(1, config_.intervalSeconds / 60); }

std::int64_t ServiceCore::NowUnix() { return ToUnixSeconds(deps_.clock.Now()); }

void ServiceCore::Log(LogEvent event, std::initializer_list<long long> numbers) {
    AppendLog(settings_.dataDir, deps_.clock.Now(), event, numbers);
}

void ServiceCore::Tick() {
    if (!running_ || suspended_) {
        return;
    }
    Snapshot current = TakeSnapshot();
    if (IsGap(current)) {
        HandleGap(current);
        return;
    }
    CleanHistoryOnNewDay(current.time);
    ProcessMinute(current);
    ++ticksInInterval_;
    if (ticksInInterval_ < TicksPerInterval()) {
        return;
    }
    FinishInterval(current, true);
    BeginInterval(current);
}

void ServiceCore::PowerSourceChanged() {
    if (!running_ || suspended_) {
        return;
    }
    Reading<bool> now = deps_.power.OnBattery();
    if (!now.ok() || now.value == onBattery_) {
        return;
    }
    Snapshot at = CloseEarly(true);
    onBattery_ = now.value;
    BeginInterval(at);
}

void ServiceCore::Suspend() {
    if (!running_ || suspended_) {
        return;
    }
    CloseEarly(false);
    suspended_ = true;
}

void ServiceCore::Resume() {
    if (!running_ || !suspended_) {
        return;
    }
    suspended_ = false;
    Reading<bool> power = deps_.power.OnBattery();
    onBattery_ = power.ok() ? power.value : onBattery_;
    BeginInterval(TakeSnapshot());
}

void ServiceCore::DisplayChanged(bool on) { displayOn_ = on; }

void ServiceCore::SessionsChanged(bool somebodyLoggedOn, bool activeSessionLocked) {
    somebodyLoggedOn_ = somebodyLoggedOn;
    activeSessionLocked_ = activeSessionLocked;
}

void ServiceCore::PipeMessage(std::string_view text, bool senderSessionActive) {
    if (!running_) {
        return;
    }
    if (!senderSessionActive) {
        ++droppedPipeMessages_;
        return;
    }
    Reading<ews::PipeMessage> parsed = ParsePipeMessage(text);
    if (!parsed.ok()) {
        ++droppedPipeMessages_;
        return;
    }
    activity_.hasMessage = true;
    activity_.idleSeconds = parsed.value.idleSeconds;
    activity_.receivedUnix = NowUnix();
    pipeBrightness_ = parsed.value.brightnessPct.has_value() ? parsed.value.brightnessPct : pipeBrightness_;
}

void ServiceCore::Stop() {
    if (!running_) {
        return;
    }
    if (!suspended_) {
        CloseEarly(false);
    }
    Log(LogEvent::ServiceStop);
    running_ = false;
    sender_.reset();
    lock_.Release();
}

}  // namespace ews
