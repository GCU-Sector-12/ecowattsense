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
#include "ews/device_name.h"
#include "ews/diag_line.h"
#include "ews/diag_recorder.h"
#include "ews/display.h"
#include "ews/emi.h"
#include "ews/energy.h"
#include "ews/history_format.h"
#include "ews/http_sender.h"
#include "ews/instance_lock.h"
#include "ews/interval.h"
#include "ews/model.h"
#include "ews/report_json.h"
#include "ews/report_queue.h"
#include "ews/server_api.h"
#include "ews/server_sync.h"
#include "ews/scan.h"
#include "ews/service_log.h"
#include "ews/service_snapshot.h"
#include "ews/status_format.h"
#include "ews/status_keep.h"
#include "ews/utc_stamp.h"
#include "ews/version.h"

namespace ews {

/// The version text written to status.json.
inline constexpr const char* kServiceVersion = kSuiteVersion;

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

    /// @param url A URL that passed IsValidServerUrl.
    /// @param key A key that passed IsValidServerKey.
    /// @return The caller of the server for the ping, the settings and the device registration.
    virtual std::unique_ptr<ServerApi> CreateApi(const std::string& url, const std::string& key) = 0;
};

/// The real factory: WinHttpSender.
class WinHttpSenderFactory final : public ISenderFactory {
public:
    std::unique_ptr<ReportSender> Create(const std::string& url, const std::string& key) override;
    std::unique_ptr<ServerApi> CreateApi(const std::string& url, const std::string& key) override;
};

/// The words of the `trigger` field of a `send` line of the diagnostics file (SPEC 7a).
inline constexpr const char* kTriggerTick = "tick";        ///< A report tick, the end of an interval, a gap, or a power source change.
inline constexpr const char* kTriggerNetwork = "network";  ///< The network came back.
inline constexpr const char* kTriggerPipe = "pipe";        ///< The pipe command `EWS1 send`.
inline constexpr const char* kTriggerCommand = "command";  ///< A command of the server (`report_now` or `flush_buffer`).

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
    IDiagWriter* diag = nullptr;  ///< Where the diagnostics lines go (SPEC 7a). Nothing is written when it is null.
    IGpuUseSource* gpuUse = nullptr;  ///< The GPU Engine counter set, read once at each tick (SPEC 5.3). Never read when it is null.
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
    /// model when its file time changed and writes status.json. Then, when a server is set and the ping interval
    /// has passed, it pings the server and runs what the answer asks (settings, commands, registration; SPEC 5.5).
    /// Ignored while suspended or after Stop.
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

    /// The network is reachable again (the shell gets the connectivity change and posts it as an event). When a
    /// server is set and the queue is not empty, the back-off is reset and a send is tried at once.
    void NetworkBack();

    /// Stop: one last read, the interval ends and its report is queued without sending, the lock is released.
    void Stop();

private:
    // Start-up steps.
    StartResult Prepare();
    /// Takes the last report time, the last send time and the last send result from the status.json of the earlier run.
    void KeepOldStatus();
    void LoadSetup();
    void BeginRun();
    void LoadSending();
    void LoadModel();
    void WriteScanFiles(const ScanResult& scan);

    // Reading and calculating.
    void TickMeasure();
    Snapshot TakeSnapshot();
    void BeginInterval(const Snapshot& at);
    Snapshot CloseEarly(bool trySend);
    void ProcessMinute(const Snapshot& current);
    /// The brightness used for a minute and where it came from.
    struct BrightnessRead {
        std::optional<double> percent;  ///< Empty when neither the service nor the pipe knows it (counted as 50).
        const char* from = "default";   ///< `service`, `pipe` or `default`.
    };
    BrightnessRead CurrentBrightness();
    std::optional<double> ReadDisplayBrightness();
    /// Reads the GPU Engine source once and gives the GPU use of the period that ends now (SPEC 4.2).
    /// @return The use in percent, or nothing when the scan did not find the source readable, there is no source,
    ///         or the read gave no usable value. A failed read never stops the tick.
    std::optional<double> ReadGpuUse();
    void AppendHistory(const Snapshot& current, double seconds, const std::optional<double>& cpuPercent,
                       const MinuteEnergy& energy, const MinuteBattery& battery);
    void RecordRow(const HistoryRow& row, UtcTime time, const MinuteBattery& battery, double seconds);
    void WriteRow(const HistoryRow& row, UtcTime time);
    struct HeldRow;
    void HoldRow(const HistoryRow& row, UtcTime time, double seconds, bool replace, bool still);
    void ResolveHeldRows(const HistoryRow& row, UtcTime time, const MinuteBattery& battery, double seconds);
    void ExpireHeldRows(std::int64_t nowUnix);
    void WriteHeldRow(HeldRow& held, const std::optional<double>& splitWatts);
    void WriteHeldRows(const std::optional<double>& splitWatts);
    void ReleaseHeldRowsAtIntervalEnd();
    void CleanHistoryOnNewDay(UtcTime now);
    bool IsGap(const Snapshot& current) const;
    void HandleGap(const Snapshot& current);

    // Diagnostics file (service_diag.cpp, SPEC 7a). A failed write never stops the work: it is counted and logged.
    /// Where the previous read of a minute came from. The first minute after a start, a resume or a gap is flagged.
    enum class PreviousRead { Normal, First, AfterGap };
    /// What one minute produced, for its tick line.
    struct DiagMinute {
        double seconds = 0.0;
        std::optional<double> cpuPercent;
        MinuteEnergy energy;      ///< As calculated (the terms are here).
        MinuteEnergy shown;       ///< As shown in the history (level L4 without a model).
        MinuteBattery battery;
        BrightnessRead brightness;
        std::optional<double> gpuPercent;  ///< The GPU use of the minute; empty when it was unknown.
    };
    /// Flags of the minute that are set while its history row is recorded.
    struct MinuteFlags {
        bool held = false;             ///< The history row of this minute was held back.
        bool batteryReplaced = false;  ///< This minute replaced one or more held impossible minutes (SPEC 4.3).
    };
    /// Writes one line through the recorder; puts `diag_full` or `diag_write_failed` into the log when the recorder says so.
    void DiagWrite(UtcTime time, const std::string& line);
    /// Writes one `event` line.
    void DiagEvent(const std::string& what, const std::vector<DiagField>& fields = {});
    /// Writes the `tick` line of the minute that was just calculated. Call it before `previous_` is replaced.
    void DiagTickLine(const Snapshot& current, const DiagMinute& minute);
    /// Writes the `report` line of a report that was just queued; does nothing when it was not queued.
    void DiagReportLine(bool queued, const std::string& reportJson);
    /// Writes the `send` line after one send try (call it after `lastSend_` is set).
    void DiagSendLine(const SendResult& result, int lines);
    /// Writes a `ping` event when the result word changed.
    void DiagPingChanged(LastPing before);
    DiagSession DiagSessionAt(std::int64_t nowUnix) const;
    DiagCalc DiagCalcOf(const Snapshot& current, const DiagMinute& minute) const;
    std::vector<std::string> DiagFlagsOf(const Snapshot& current, const DiagMinute& minute) const;

    // Reports, queue, sending, status.
    void FinishInterval(const Snapshot& at, bool trySend);
    ReportFields MakeFields(const IntervalResult& result);
    void QueueReport(const IntervalResult& result);
    void LogQueueOutcome(const QueueAppendResult& queued);
    void FlushCounters();
    void FlushCounter(LogEvent event, int& count);
    void SendQueued();
    /// Resets the back-off, tries a send now and writes status.json, when a server is set and the queue is not empty.
    /// @param event The log event that tells why (SendOnNetwork or SendRequested).
    /// @return True when a send was tried.
    bool SendNow(LogEvent event);
    /// Like SendNow, and the `send` lines of the diagnostics file name the trigger.
    /// @param trigger One of kTriggerNetwork, kTriggerPipe and kTriggerCommand.
    /// @param event The log event that tells why.
    /// @return True when a send was tried.
    bool SendNowFor(const char* trigger, LogEvent event);
    /// Handles `EWS1 send`: at most one send in 30 seconds; a later one is dropped and counted.
    void SendRequested();
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

    // Server sync (service_sync.cpp): settings from the server, the ping, the commands and the registration.
    enum class PullResult { Applied, Ignored, Failed };
    /// Reads `<data>\status\server-settings.json` and `commands-done.json` at start; the settings win over service.ini.
    void LoadSyncFiles();
    /// Reads GET /api/settings once at start, when a server is set.
    void PullSettingsAtStart();
    /// Counts the tick for the ping and pings when it is due and no `Retry-After` hold is running.
    void PingTick();
    /// Sends POST /api/ping and handles the answer.
    void Ping();
    /// Reads GET /api/settings and applies a good answer at once.
    /// @return Applied; Ignored when a value was bad or missing (the old settings stay); Failed when there was no good answer.
    PullResult PullSettings();
    /// Puts the settings in force: the report interval, the thresholds, the ping interval.
    void ApplySettings(const ServerSettings& settings);
    void SaveSettingsFile();
    void NotePing(ApiWord word, const ApiAnswer& answer);
    void HandlePing(const PingAnswer& answer, const std::vector<DoneCommand>& sentAcks);
    void CheckSettingsVersion(std::int64_t version);
    void RunCommands(const std::vector<ServerCommand>& commands);
    void RunOneCommand(const ServerCommand& command);
    CommandResult Execute(const ServerCommand& command);
    CommandResult CommandSetInterval(const ServerCommand& command);
    CommandResult CommandReportNow();
    CommandResult CommandFlushBuffer();
    CommandResult CommandClearBuffer();
    CommandResult CommandPullSettings();
    void SaveDoneCommands();
    /// Sends the computer name when the ping said registered false, at most once an hour.
    /// @return True when a PUT was sent.
    bool RegisterDevice();
    /// Handles `EWS1 name=<base64>`: checks the name and the 10 second limit, stores the name file, then sends it.
    /// A bad or too early message is dropped and counted.
    /// @param text The bytes of the message (untrusted).
    void NameRequested(std::string_view text);
    /// @return The first line of `<data>\Client\device-name.txt` as a name, or nothing when there is none.
    std::optional<std::string> ReadLocalName();
    /// Writes (or, for no name, deletes) `<data>\Client\device-name.txt`.
    /// @param name The checked name, or nothing.
    /// @return True when the file now holds the name (or is gone).
    bool StoreLocalName(const std::optional<std::string>& name);
    /// Sends PUT /api/devices/{device_id} with the local name and notes the answer (server name, name_sync, back-off).
    void PutName();
    /// Sends the PUT again when the name is pending, no PUT went out in this ping, and no Retry-After hold runs.
    /// @param putSent True when a PUT was already sent in this ping.
    void RetryNameSync(bool putSent);
    /// Notes the answer of a PUT: the server name, the name_sync state and the log line.
    void NoteNameAnswer(const ApiAnswer& answer);
    /// Sets name_sync and writes `<data>\status\name-sync.json` when it changed.
    void SetNameSync(NameSync state);
    /// Keeps the next ping back for the `Retry-After` of a 429 or 503 answer.
    void Hold(const ApiAnswer& answer);
    int PingTicks() const;
    int UnsentCount();

    ServiceDeps deps_;
    ServiceSettings settings_;
    DiagRecorder diag_;
    InstanceLock lock_;
    ServiceConfig config_;
    std::string deviceId_;
    std::unique_ptr<ReportSender> sender_;
    std::unique_ptr<ServerApi> api_;  ///< Set with sender_: a server URL and a key are known.
    bool keySet_ = false;  ///< True when key.bin was read and decrypted at start (shown in status.json, never the key).
    std::vector<EmiChannel> channels_;
    std::optional<CalibrationModel> model_;
    std::optional<std::int64_t> modelFileTime_;
    MeasurementLevel scanLevel_ = MeasurementLevel::L4;
    bool displayReadable_ = false;
    bool gpuUseReadable_ = false;  ///< The scan in the service context found the source `gpu_use` readable (SPEC 3.2).
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
    PreviousRead previousRead_ = PreviousRead::First;  ///< The first read is the one taken at the start.
    MinuteFlags minuteFlags_;
    const char* sendTrigger_ = kTriggerTick;
    std::optional<IntervalAccumulator> interval_;
    int ticksInInterval_ = 0;
    std::string lastDate_;
    bool historyFailing_ = false;

    /// A history row held back: its battery change was impossible (SPEC 4.3), or the gauge did not move on battery
    /// (SPEC 7), or it came after such a row.
    struct HeldRow {
        HistoryRow row;
        UtcTime time{};
        double seconds = 0.0;  ///< The exact length of the minute; the split of a fall is in proportion to it.
        bool replace = false;  ///< True for the impossible minute itself: its battery power is replaced when a possible minute follows.
        bool still = false;    ///< True for a minute on battery whose charge did not fall: it shares the next fall.
    };
    std::vector<HeldRow> heldRows_;
    /// What the held rows add up to, for the split of a fall.
    struct HeldTotals {
        double stillSeconds = 0.0;
        int stillRows = 0;
        int jumpRows = 0;  ///< Held impossible minutes (SPEC 4.3).
    };
    HeldTotals SummariseHeld() const;
    int batteryJumps_ = 0;     ///< Impossible battery changes since the last report.
    int batteryReplaced_ = 0;  ///< Replacements since the last report.

    std::optional<ServerSettings> serverSettings_;  ///< The settings that came from the server; empty while service.ini rules.
    std::optional<std::int64_t> ignoredVersion_;    ///< A settings version whose answer was bad: not read again until the version changes.
    std::optional<std::int64_t> lastSettingsUnix_;
    std::optional<std::int64_t> lastPingUnix_;
    std::optional<std::int64_t> lastRegisterUnix_;
    LastPing lastPing_ = LastPing::Never;
    std::vector<DoneCommand> doneCommands_;
    bool doneDirty_ = false;
    int pingIntervalSeconds_ = 60;
    int ticksSincePing_ = 1 << 20;  ///< Large at start, so the first tick pings.
    int pingHold_ = 0;              ///< Ticks to wait for a `Retry-After`.
    int pingFailures_ = 0;
    int settingsIgnored_ = 0;

    int sendFailures_ = 0;
    int waitIntervals_ = 0;
    LastSend lastSend_ = LastSend::Never;
    bool sendReasonLogged_ = false;
    std::optional<std::int64_t> lastSendUnix_;
    std::optional<std::int64_t> lastReportUnix_;
    std::optional<std::int64_t> lastSendRequestUnix_;  ///< When the last `EWS1 send` caused a send.
    int droppedPipeMessages_ = 0;
    NameSync nameSync_ = NameSync::None;                ///< Where the name stands with the server (name-sync.json).
    bool serverNameKnown_ = false;                      ///< True after a PUT answer that named the server side.
    std::optional<std::string> serverName_;             ///< The name the PUT answer said the server holds; empty for null.
    std::optional<std::int64_t> lastNameRequestUnix_;   ///< When the last good `EWS1 name=` message was taken.
    int skippedReports_ = 0;
    std::uint32_t lastQueued_ = 0;
};

}  // namespace ews
