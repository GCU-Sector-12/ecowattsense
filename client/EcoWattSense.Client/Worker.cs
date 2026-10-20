using EcoWattSense.Client.Monitoring;

namespace EcoWattSense.Client;

public class Worker(ILogger<Worker> logger, WindowsActivityMonitor activityMonitor, CpuMonitor cpuMonitor,
    WindowsSessionMonitor sessionMonitor) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        while (!stoppingToken.IsCancellationRequested)
        {
            double? inactivitySeconds = null;
            double? cpuPercentage = null;
            bool? isLocked = null;
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
                cpuPercentage = cpuMonitor.GetUsagePercentage();
            }
            catch (InvalidOperationException exception)
            {
                logger.LogWarning(exception, "Unable to read CPU usage; will retry in 5 seconds.");
            }

            try
            {
                isLocked = sessionMonitor.IsLocked();
            }
            catch (InvalidOperationException exception)
            {
                logger.LogWarning(exception, "Unable to read session-lock status; will retry in 5 seconds.");
            }

            logger.LogInformation(
                "User inactivity: {InactivitySeconds} seconds; CPU usage: {CpuUsagePercent}; Screen lock: {ScreenLockStatus}",
                inactivitySeconds?.ToString("F1", System.Globalization.CultureInfo.InvariantCulture) ?? "unavailable",
                cpuPercentage is { } cpu
                    ? cpu.ToString("F1", System.Globalization.CultureInfo.InvariantCulture) + "%"
                    : "unavailable (warming up or no valid sample)",
                isLocked switch { true => "locked", false => "unlocked", null => "unknown" });

            await Task.Delay(TimeSpan.FromSeconds(5), stoppingToken);
        }
    }
}
