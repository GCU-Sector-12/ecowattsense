using EcoWattSense.Client;
using EcoWattSense.Client.Monitoring;
using EcoWattSense.Client.Reporting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("Live monitoring checks require Windows.");
string directory = Path.Combine(Path.GetTempPath(), "EcoWattSense-Integration-" + Guid.NewGuid().ToString("N"));
try
{
    var builder = Host.CreateApplicationBuilder();
    builder.Logging.ClearProviders();
    builder.Configuration.Sources.Clear();
    builder.Configuration.AddInMemoryCollection(new Dictionary<string, string?>
    {
        ["Server:Enabled"] = "false",
        ["DeviceIdentity:Directory"] = directory,
        ["History:Directory"] = Path.Combine(directory, "History"),
        ["Reporting:IntervalSeconds"] = "300"
    });
    int prompts = 0;
    using var host = await MonitoringHost.CreateAsync(builder, () => { prompts++; return "laptop"; });
    Check(prompts == 1, "First startup prompts once");
    Check(DeviceTypeSelection.Resolve(null, directory, () => throw new Exception("Unexpected prompt")) == "laptop", "Saved choice reused");
    Check(DeviceTypeSelection.Resolve("desktop", directory, () => throw new Exception("Unexpected prompt")) == "desktop", "Configured choice reused");
    Check(File.ReadAllText(Path.Combine(directory, "device-type.txt")).Trim() == "laptop", "Override preserves saved choice");
    var worker = host.Services.GetRequiredService<Worker>();
    Check(ReferenceEquals(worker, host.Services.GetServices<IHostedService>().Single()), "Exactly one hosted worker");
    Check(host.Services.GetRequiredService<ReportingSettings>().IntervalSeconds == 300, "Report interval unchanged");
    var readings = new List<MonitoringReading>();
    var secondReading = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
    worker.ReadingAvailable += _ => throw new InvalidOperationException("Observer failure must not stop worker");
    worker.ReadingAvailable += reading => { readings.Add(reading); if (readings.Count == 2) secondReading.TrySetResult(); };
    await host.StartAsync();
    await worker.FirstReading.WaitAsync(TimeSpan.FromSeconds(10));
    await secondReading.Task.WaitAsync(TimeSpan.FromSeconds(12));
    Check((readings[1].Timestamp - readings[0].Timestamp).TotalSeconds >= 4.5, "Existing five-second sampling cadence");
    Check(readings[1].State == new StateClassifier().Classify(readings[1].InactivitySeconds), "Live state uses existing classifier");
    await host.StopAsync();
    Check(worker.ExecuteTask!.IsCompleted, "Worker completes on stop");
    Console.WriteLine("Passed shared-host integration checks (two native Windows readings and clean shutdown).");
}
finally
{
    if (Directory.Exists(directory)) Directory.Delete(directory, true);
}
static void Check(bool success, string message)
{
    if (!success) throw new Exception(message);
    Console.WriteLine("PASS: " + message);
}
