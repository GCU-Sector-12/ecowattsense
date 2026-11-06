using System.Net;
using System.Net.Sockets;
using System.Text;
using EcoWattSense.Client;
using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Monitoring;
using EcoWattSense.Client.Reporting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("Live monitoring checks require Windows.");
string directory = Path.Combine(Path.GetTempPath(), "EcoWattSense-Integration-" + Guid.NewGuid().ToString("N"));
// Answers every request with HTTP 401 and counts the connections.
using var unauthorized = new TcpListener(IPAddress.Loopback, 0);
unauthorized.Start();
int serverCalls = 0;
_ = Task.Run(async () =>
{
    while (true)
    {
        using var client = await unauthorized.AcceptTcpClientAsync();
        Interlocked.Increment(ref serverCalls);
        var stream = client.GetStream();
        var buffer = new byte[8192];
        var request = new StringBuilder();
        while (!request.ToString().Contains("\r\n\r\n"))
        {
            int read = await stream.ReadAsync(buffer);
            if (read == 0) break;
            request.Append(Encoding.ASCII.GetString(buffer, 0, read));
        }
        await stream.WriteAsync(Encoding.ASCII.GetBytes(
            "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));
    }
});
string unauthorizedUrl = $"http://127.0.0.1:{((IPEndPoint)unauthorized.LocalEndpoint).Port}";
try
{
    try
    {
        using var unexpected = await MonitoringHost.CreateAsync(NewBuilder(unauthorizedUrl),
            () => throw new Exception("Prompted after listener setup failed"),
            onLockAcquired: _ => Task.FromException(new IOException("Test listener failure")));
        throw new Exception("Started after listener setup failed");
    }
    catch (IOException ex) when (ex.Message == "Test listener failure") { }
    Check(serverCalls == 0, "Listener setup failure precedes server calls");
    CheckLockFree("Lock is free after listener setup fails");
    {
        int prompts = 0;
        using var host = await MonitoringHost.CreateAsync(NewBuilder(), () => { prompts++; return "laptop"; });
        Check(prompts == 1, "First startup prompts once");
        // A second copy while the first host is alive: refused before the prompt and before the server.
        try
        {
            using var second = await MonitoringHost.CreateAsync(NewBuilder(unauthorizedUrl), () => { prompts++; return "laptop"; });
            throw new Exception("Second copy started monitoring");
        }
        catch (ClientAlreadyRunningException) { }
        Check(prompts == 1 && serverCalls == 0, "Second copy is refused without prompt or server call");
        await RunMonitoringChecksAsync(host);
    }
    CheckLockFree("Lock is free after the host is disposed");

    // Forget the saved device type, so the next startup asks again and the prompt can be cancelled.
    File.Delete(Path.Combine(directory, "device-type.txt"));
    int cancelledPrompts = 0;
    try
    {
        using var cancelled = await MonitoringHost.CreateAsync(NewBuilder(),
            () => { cancelledPrompts++; throw new InvalidOperationException("Device type selection was cancelled."); });
        throw new Exception("Cancelled prompt started monitoring");
    }
    catch (InvalidOperationException) { }
    Check(cancelledPrompts == 1, "Cancelled prompt was shown");
    CheckLockFree("Lock is free after a cancelled device type prompt");

    try
    {
        using var rejected = await MonitoringHost.CreateAsync(NewBuilder(unauthorizedUrl), () => "laptop");
        throw new Exception("HTTP 401 from settings started monitoring");
    }
    catch (InvalidOperationException) { }
    Check(serverCalls == 1, "Settings request reached the 401 server");
    CheckLockFree("Lock is free after HTTP 401 from GET /api/settings");
    Console.WriteLine("Passed single-instance host checks (second copy refused, lock freed after dispose and after failed startups).");
}
finally
{
    unauthorized.Stop();
    if (Directory.Exists(directory)) Directory.Delete(directory, true);
}

HostApplicationBuilder NewBuilder(string? serverUrl = null)
{
    var builder = Host.CreateApplicationBuilder();
    builder.Logging.ClearProviders();
    builder.Configuration.Sources.Clear();
    builder.Configuration.AddInMemoryCollection(new Dictionary<string, string?>
    {
        ["Server:Enabled"] = serverUrl is null ? "false" : "true",
        ["Server:BaseUrl"] = serverUrl,
        ["Server:ClientKey"] = serverUrl is null ? null : "dummy-test-key",
        ["DeviceIdentity:Directory"] = directory,
        ["History:Directory"] = Path.Combine(directory, "History"),
        ["Reporting:IntervalSeconds"] = "300"
    });
    return builder;
}

void CheckLockFree(string message)
{
    using (SingleInstanceLock.Acquire(directory)) { }
    Check(true, message);
}

async Task RunMonitoringChecksAsync(IHost host)
{
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
static void Check(bool success, string message)
{
    if (!success) throw new Exception(message);
    Console.WriteLine("PASS: " + message);
}
