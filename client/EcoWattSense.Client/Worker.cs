using EcoWattSense.Client.Monitoring;

namespace EcoWattSense.Client;

public class Worker(ILogger<Worker> logger, WindowsActivityMonitor activityMonitor, CpuMonitor cpuMonitor) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        while (!stoppingToken.IsCancellationRequested)
        {
            double? inactivitySeconds = null;
            double? cpuPercentage = null;
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

            logger.LogInformation(
                "User inactivity: {InactivitySeconds} seconds; CPU usage: {CpuUsagePercent}",
                inactivitySeconds?.ToString("F1", System.Globalization.CultureInfo.InvariantCulture) ?? "unavailable",
                cpuPercentage is { } cpu
                    ? cpu.ToString("F1", System.Globalization.CultureInfo.InvariantCulture) + "%"
                    : "unavailable (warming up or no valid sample)");

            await Task.Delay(TimeSpan.FromSeconds(5), stoppingToken);
        }
    }
}
