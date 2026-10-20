using EcoWattSense.Client.Monitoring;
using EcoWattSense.Client.Reporting;
using System.Diagnostics;

namespace EcoWattSense.Client;

public class Worker(ILogger<Worker> logger, WindowsActivityMonitor activityMonitor, CpuMonitor cpuMonitor,
    WindowsSessionMonitor sessionMonitor, WindowsPowerMonitor powerMonitor,
    StateClassifier stateClassifier, ReportInterval reportInterval) : BackgroundService
{
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
                logger.LogInformation("Report generated locally (not sent or buffered): {ReportJson}", ReportJson.Serialize(report));

            await Task.Delay(TimeSpan.FromSeconds(5), stoppingToken);
        }
    }
}
