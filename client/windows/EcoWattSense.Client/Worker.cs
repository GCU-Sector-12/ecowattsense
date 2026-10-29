using EcoWattSense.Client.Monitoring;
using EcoWattSense.Client.Reporting;
using System.Diagnostics;
using EcoWattSense.Client.Communication;
using EcoWattSense.Client.Identity;

namespace EcoWattSense.Client;

public class Worker(ILogger<Worker> logger, WindowsActivityMonitor activityMonitor, CpuMonitor cpuMonitor,
    WindowsSessionMonitor sessionMonitor, WindowsPowerMonitor powerMonitor,
    StateClassifier stateClassifier, ReportInterval reportInterval, ServerClient server,
    TextReportWriter textReportWriter, JsonReportWriter jsonReportWriter,
    ActivityHistoryWriter activityHistory, DeviceIdentity identity, ReportingSettings reportingSettings) : BackgroundService
{
    public event Action<MonitoringReading>? ReadingAvailable;
    private readonly TaskCompletionSource firstReading = new(TaskCreationOptions.RunContinuationsAsynchronously);
    public Task FirstReading => firstReading.Task;

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        long clockOrigin = Stopwatch.GetTimestamp();
        while (!stoppingToken.IsCancellationRequested)
        {
            double? inactivitySeconds = null;
            double? cpuPercentage = null;
            CpuReading? cpuReading = null;
            bool? isLocked = null;
            bool? isOnBattery = null;

            // CPU
            try
            {
                inactivitySeconds = activityMonitor.GetInactivitySeconds();
            }
            catch (InvalidOperationException exception)
            {
                logger.LogWarning(exception, "Unable to read user inactivity; will retry in 5 seconds.");
            }

            try
            {
                cpuReading = cpuMonitor.GetSample();
                cpuPercentage = cpuReading.Sample?.UsagePercentage;
            }
            catch (InvalidOperationException exception)
            {
                logger.LogWarning(exception, "Unable to read CPU usage; will retry in 5 seconds.");
            }

            // Capture the boundary adjacent to the one CPU measurement, not after logging.
            TimeSpan cpuBoundary = Stopwatch.GetElapsedTime(clockOrigin);
            DateTime utcEnd = DateTime.UtcNow;

            // Screen lock and power
            try
            {
                isLocked = sessionMonitor.IsLocked();
            }
            catch (InvalidOperationException exception)
            {
                logger.LogWarning(exception, "Unable to read session-lock status; will retry in 5 seconds.");
            }

            try
            {
                isOnBattery = powerMonitor.IsOnBattery();
            }
            catch (InvalidOperationException exception)
            {
                logger.LogWarning(exception, "Unable to read power status; will retry in 5 seconds.");
            }

            ActivityState? state = stateClassifier.Classify(inactivitySeconds);
            var reading = new MonitoringReading(utcEnd, inactivitySeconds, cpuPercentage, state, isLocked, isOnBattery);
            // Observers must never interrupt reporting or sampling.
            if (ReadingAvailable is { } observers)
                foreach (Action<MonitoringReading> observer in observers.GetInvocationList())
                    try { observer(reading); }
                    catch (Exception exception) { logger.LogWarning(exception, "Live reading observer failed."); }
            firstReading.TrySetResult();

            try
            {
                activityHistory.Save(identity.Id, reportingSettings.DeviceType, utcEnd,
                    inactivitySeconds, cpuPercentage, state, isLocked, isOnBattery);
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or System.Text.Json.JsonException)
            {
                logger.LogError(exception, "ACTIVITY HISTORY NOT SAVED: {HistoryPath}", activityHistory.FilePath);
            }

            logger.LogInformation(
                "User inactivity: {InactivitySeconds} seconds; CPU usage: {CpuUsagePercent}; Screen lock: {ScreenLockStatus}; Power source: {PowerSource}; State: {ActivityState}",
                inactivitySeconds?.ToString("F1", System.Globalization.CultureInfo.InvariantCulture) ?? "unavailable",
                cpuPercentage is { } cpu
                    ? cpu.ToString("F1", System.Globalization.CultureInfo.InvariantCulture) + "%"
                    : "unavailable (warming up or no valid sample)",
                isLocked switch { true => "locked", false => "unlocked", null => "unknown" },
                isOnBattery switch { true => "battery", false => "mains", null => "unknown" },
                state?.ToString() ?? "unknown");

            var result = reportInterval.Observe(cpuBoundary, utcEnd, cpuReading,
                inactivitySeconds, state, isLocked, isOnBattery);
            if (result.SkipReason is { } reason)
                logger.LogWarning("Report skipped: {Reason}", reason);
            if (result.Report is { } report)
            {
                try
                {
                    string historyPath = jsonReportWriter.Save(report);
                    logger.LogInformation("JSON report saved to {ReportPath}; latest copy: {LatestPath}", historyPath, jsonReportWriter.LatestPath);
                }
                catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
                {
                    logger.LogWarning(exception, "Unable to save JSON report; monitoring continues.");
                }
                try
                {
                    textReportWriter.Save(report);
                    logger.LogInformation("Readable report saved to {ReportPath}", textReportWriter.FilePath);
                }
                catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
                {
                    logger.LogWarning(exception, "Unable to save the readable report; monitoring continues.");
                }
                if (!server.Enabled)
                    logger.LogInformation("Report generated locally (not sent or buffered): {ReportJson}", ReportJson.Serialize(report));
                else
                {
                    try
                    {
                        var sent = await server.SendReportAsync(report, stoppingToken);
                        if (sent.Outcome is ReportOutcome.Accepted or ReportOutcome.Duplicate)
                            logger.LogInformation("Report {DeviceId} at {Timestamp:O}: {Outcome}", report.DeviceId, report.Timestamp, sent.Outcome);
                        else
                            logger.LogWarning("Report {DeviceId} at {Timestamp:O}: {Outcome}; dropped without retry or buffering ({Reason})",
                                report.DeviceId, report.Timestamp, sent.Outcome, sent.Reason);
                    }
                    catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
                    {
                        logger.LogInformation("Report {DeviceId} at {Timestamp:O}: dropped on shutdown; acceptance unknown, no buffering",
                            report.DeviceId, report.Timestamp);
                        return;
                    }
                }
            }

            await Task.Delay(TimeSpan.FromSeconds(5), stoppingToken);
        }
    }
}
