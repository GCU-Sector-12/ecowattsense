using EcoWattSense.Client;
using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Monitoring;
using EcoWattSense.Client.Reporting;
using EcoWattSense.Client.Communication;

var builder = Host.CreateApplicationBuilder(args);
using var server = new ServerClient(new ServerOptions(
    builder.Configuration.GetValue<bool>("Server:Enabled", false),
    builder.Configuration["Server:BaseUrl"], builder.Configuration["Server:ClientKey"]));
builder.Services.AddSingleton(server);
StartupSettings? remote = null;
if (server.Enabled)
{
    using var startupCancellation = new CancellationTokenSource();
    ConsoleCancelEventHandler cancel = (_, e) => { e.Cancel = true; startupCancellation.Cancel(); };
    Console.CancelKeyPress += cancel;
    try
    {
        remote = await server.GetSettingsAsync(startupCancellation.Token);
        if (remote.FallbackReason is { } reason)
            Console.Error.WriteLine($"Startup settings unavailable ({reason}); using local defaults 300/600/3600. No retry.");
    }
    catch (OperationCanceledException) when (startupCancellation.IsCancellationRequested) { return; }
    finally { Console.CancelKeyPress -= cancel; }
}
builder.Services.AddSingleton(new ReportingSettings(
    remote?.ReportSeconds ?? builder.Configuration.GetValue<int>("Reporting:IntervalSeconds", 300),
    builder.Configuration["Reporting:DeviceType"] ?? ""));
builder.Services.AddSingleton<ReportInterval>();
// Read and validate local thresholds at startup, before starting any monitoring.
builder.Services.AddSingleton(new StateClassifier(
    remote?.IdleSeconds ?? builder.Configuration.GetValue<double>("StateClassification:IdleThresholdSeconds", 600),
    remote?.UnusedSeconds ?? builder.Configuration.GetValue<double>("StateClassification:UnusedThresholdSeconds", 3600)));
var identityDirectory = DeviceIdentityStore.ResolveDirectory(builder.Configuration["DeviceIdentity:Directory"]);
var identityResult = new DeviceIdentityStore().LoadOrCreate(identityDirectory);
builder.Services.AddSingleton(identityResult.Identity);
builder.Services.AddSingleton<WindowsActivityMonitor>();
builder.Services.AddSingleton<CpuMonitor>();
builder.Services.AddSingleton<WindowsSessionMonitor>();
builder.Services.AddSingleton<WindowsPowerMonitor>();
builder.Services.AddHostedService<Worker>();

using var host = builder.Build();
var reportingSettings = host.Services.GetRequiredService<ReportingSettings>();
var classifier = host.Services.GetRequiredService<StateClassifier>();
string settingsSource = remote is null ? "local configuration"
    : remote.FallbackReason is null ? "server" : "fallback defaults";
host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("StartupSettings")
    .LogInformation("Effective settings: reporting interval {ReportIntervalSeconds} seconds; idle threshold {IdleThresholdSeconds} seconds; unused threshold {UnusedThresholdSeconds} seconds; source: {SettingsSource}",
        reportingSettings.IntervalSeconds, classifier.IdleThresholdSeconds,
        classifier.UnusedThresholdSeconds, settingsSource);
host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("DeviceIdentity")
    .LogInformation("Device identity {DeviceId} {IdentityAction} from {IdentityDirectory}",
        identityResult.Identity.Id, identityResult.Created ? "created" : "loaded", identityDirectory);
host.Run();
