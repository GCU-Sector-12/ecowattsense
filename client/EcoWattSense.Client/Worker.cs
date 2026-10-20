using EcoWattSense.Client.Monitoring;

namespace EcoWattSense.Client;

public class Worker(ILogger<Worker> logger, WindowsActivityMonitor activityMonitor) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                double inactivitySeconds = activityMonitor.GetInactivitySeconds();
                logger.LogInformation("User inactivity: {InactivitySeconds:F1} seconds", inactivitySeconds);
            }
            catch (InvalidOperationException exception)
            {
                logger.LogWarning(exception, "Unable to read user inactivity; will retry in 5 seconds.");
            }

            await Task.Delay(TimeSpan.FromSeconds(5), stoppingToken);
        }
    }
}
