// service_core.h: the loop of the service (SPEC 5.3, 4.4, 4.5 and 6) as a class with everything injected.
// Used by ews-service (the program is a thin shell around it) and by the tests, which give it fakes and a temporary
// data folder. The class runs on ONE thread: the shell calls the entry points one after another, so a session event
// that arrives during a tick waits until the tick is done. It never waits and owns no timer; the shell owns the timer.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ews/activity.h"
#include "ews/battery.h"
#include "ews/config.h"
#include "ews/cpu_times.h"
#include "ews/display.h"
#include "ews/emi.h"
#include "ews/energy.h"
#include "ews/http_sender.h"
#include "ews/instance_lock.h"
#include "ews/interval.h"
#include "ews/model.h"
#include "ews/report_json.h"
#include "ews/report_queue.h"
#include "ews/scan.h"
#include "ews/service_log.h"
#include "ews/service_snapshot.h"
#include "ews/status_format.h"
#include "ews/utc_stamp.h"

namespace ews {

/// The version text written to status.json.
inline constexpr const char* kServiceVersion = "0.1.0";

/// The clock. Tests use a fake that they move by hand.
class IClock {
public:
    virtual ~IClock() = default;
    /// @return The current UTC time.
    virtual UtcTime Now() = 0;
};

/// The real clock (system_clock).
class SystemClock final : public IClock {
public:
    UtcTime Now() override;
};

/// The power source (mains or battery). Tests use a fake.
class IPowerSource {
public:
    virtual ~IPowerSource() = default;
    /// @return Ok with true for battery, false for mains; a failure when the state is unknown.
    virtual Reading<bool> OnBattery() = 0;
};

/// The real power source (ReadOnBattery).
class SystemPowerSource final : public IPowerSource {
public:
    Reading<bool> OnBattery() override;
};

/// Runs the scan of all sources. Tests give a canned result; the real runner takes about 3 seconds.
class IScanRunner {
public:
    virtual ~IScanRunner() = default;
    /// @param context The context to record.
    /// @return The scan result.
    virtual ScanResult Run(ScanContext context) = 0;
};

/// The real scan runner: the system sources and RunScan.
class SystemScanRunner final : public IScanRunner {
public:
    ScanResult Run(ScanContext context) override;
};

/// Makes the sender once the URL and the key are known. Tests give a fake sender.
class ISenderFactory {
public:
    virtual ~ISenderFactory() = default;
    /// @param url A URL that passed IsValidServerUrl.
    /// @param key A key that passed IsValidServerKey.
    /// @return The sender.
    virtual std::unique_ptr<ReportSender> Create(const std::string& url, const std::string& key) = 0;
};

/// The real factory: WinHttpSender.
class WinHttpSenderFactory final : public ISenderFactory {
public:
    std::unique_ptr<ReportSender> Create(const std::string& url, const std::string& key) override;
};

/// The sources and helpers of the core. The caller owns them; they must outlive the core.
struct ServiceDeps {
    IClock& clock;
    IScanRunner& scan;
    IEmiSource& emi;
    IBatterySource& battery;
    ICpuTimesSource& cpu;
    IDisplaySource& display;
    IPowerSource& power;
    ISenderFactory& senders;
};

/// The settings of the core.
struct ServiceSettings {
    std::wstring dataDir;  ///< The data folder (`<data>` of SPEC 2.2); it must exist.
    int tickSeconds = 60;  ///< The length of one tick; a tick that comes after more than 3 of them is a gap.
};

/// How Start ended.
enum class StartResult {
    Ok,
    DataFolderMissing,  ///< The data folder does not exist (nothing is created).
    LockHeld,           ///< Another program holds Client\instance.lock.
    LockFailed,         ///< The lock file could not be opened for another reason.
    IdentityFailed      ///< device-id.txt is bad or cannot be read or written.
};

/// @param result A start result.
/// @return A fixed word: ok, data_folder_missing, lock_held, lock_failed or identity_failed.
const char* StartResultWord(StartResult result);

/// The service loop. One instance runs from Start to Stop.
class ServiceCore {
public:
    /// @param deps The sources and helpers.
    /// @param settings The data folder and the tick length.
    ServiceCore(const ServiceDeps& deps, ServiceSettings settings);

    /// Takes the lock, loads the identity, scans (context service), loads the configuration, the key and the model,
    /// takes the first read of every counter and starts the first interval. Writes the first status.json.
    /// @return Ok, or the reason the service must not run. On failure nothing is held.
    StartResult Start();

    /// One tick: reads the counters, calculates the minute, appends a history row. At the end of an interval
    /// (every `interval_seconds / 60` ticks) it also builds the report, queues it, sends when allowed, reloads the
    /// model when its file time changed and writes status.json. Ignored while suspended or after Stop.
    void Tick();

    /// The power source changed: one read at once, the interval ends early and its report is queued and sent.
    void PowerSourceChanged();

    /// The computer goes to sleep: one read, the interval ends early, its report is queued and NOT sent.
    void Suspend();

    /// The computer woke up: a new first read and a new interval. No report covers the time asleep.
    void Resume();

    /// The display went on or off.
    /// @param on True when the display is on.
    void DisplayChanged(bool on);

    /// The session list changed (the shell works the two facts out; the core never polls).
    /// @param somebodyLoggedOn At least one session is active and has a user name.
    /// @param activeSessionLocked The active session is locked.
    void SessionsChanged(bool somebodyLoggedOn, bool activeSessionLocked);

    /// A message arrived on the session pipe. Untrusted text.
    /// @param text The bytes of the message.
    /// @param senderSessionActive True when the shell found the session of the sender in the state active.
    void PipeMessage(std::string_view text, bool senderSessionActive);

    /// Stop: one last read, the interval ends and its report is queued without sending, the lock is released.
    void Stop();

private:
    // Start-up steps.
    StartResult Prepare();
    void LoadSetup();
    void BeginRun();
    void LoadSending();
    void LoadModel();
    void WriteScanFiles(const ScanResult& scan);

    // Reading and calculating.
    Snapshot TakeSnapshot();
    void BeginInterval(const Snapshot& at);
    Snapshot CloseEarly(bool trySend);
    void ProcessMinute(const Snapshot& current);
    std::optional<double> CurrentBrightness();
    std::optional<double> ReadDisplayBrightness();
    void AppendHistory(const Snapshot& current, double seconds, const std::optional<double>& cpuPercent,
                       const MinuteEnergy& energy, const std::optional<double>& batteryChangeUj);
    void CleanHistoryOnNewDay(UtcTime now);
    bool IsGap(const Snapshot& current) const;
    void HandleGap(const Snapshot& current);

    // Reports, queue, sending, status.
    void FinishInterval(const Snapshot& at, bool trySend);
    ReportFields MakeFields(const IntervalResult& result);
    void QueueReport(const IntervalResult& result);
    void LogQueueOutcome(const QueueAppendResult& queued);
    void FlushCounters();
    void SendQueued();
    bool SendOneBatch();
    void HandleSendResult(const SendResult& result, int lines);
    void LogSend(const SendResult& result, int lines);
    void NoteDisabled();
    void ReloadModelIfChanged();
    void WriteStatus();
    void CheckLevelChange();
    int EffectiveLevelNumber() const;
    int TicksPerInterval() const;
    std::int64_t NowUnix();
    void Log(LogEvent event, std::initializer_list<long long> numbers = {});

    ServiceDeps deps_;
    ServiceSettings settings_;
    InstanceLock lock_;
    ServiceConfig config_;
    std::string deviceId_;
    std::unique_ptr<ReportSender> sender_;
    std::vector<EmiChannel> channels_;
    std::optional<CalibrationModel> model_;
    std::optional<std::int64_t> modelFileTime_;
    MeasurementLevel scanLevel_ = MeasurementLevel::L4;
    bool displayReadable_ = false;
    int loggedLevel_ = 0;

    bool running_ = false;
    bool suspended_ = false;
    bool onBattery_ = false;
    bool displayOn_ = true;
    bool somebodyLoggedOn_ = false;
    bool activeSessionLocked_ = false;
    SessionActivity activity_;
    std::optional<int> pipeBrightness_;
    std::int64_t serviceStartUnix_ = 0;

    Snapshot previous_;
    std::optional<IntervalAccumulator> interval_;
    int ticksInInterval_ = 0;
    std::string lastDate_;
    bool historyFailing_ = false;

    int sendFailures_ = 0;
    int waitIntervals_ = 0;
    LastSend lastSend_ = LastSend::Never;
    bool sendReasonLogged_ = false;
    std::optional<std::int64_t> lastSendUnix_;
    std::optional<std::int64_t> lastReportUnix_;
    int droppedPipeMessages_ = 0;
    int skippedReports_ = 0;
    std::uint32_t lastQueued_ = 0;
};

}  // namespace ews
