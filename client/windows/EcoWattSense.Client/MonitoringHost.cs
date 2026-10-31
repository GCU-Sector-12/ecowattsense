using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Monitoring;
using EcoWattSense.Client.Reporting;
using EcoWattSense.Client.Communication;

namespace EcoWattSense.Client;

public static class MonitoringHost
{
    public static async Task<IHost> CreateAsync(HostApplicationBuilder builder, Func<string> selectDeviceType, CancellationToken cancellationToken = default)
    {
        var identityDirectory = DeviceIdentityStore.ResolveDirectory(builder.Configuration["DeviceIdentity:Directory"]);
        var deviceType = DeviceTypeSelection.Resolve(builder.Configuration["Reporting:DeviceType"],
        identityDirectory, selectDeviceType);
        var server = new ServerClient(new ServerOptions(
        builder.Configuration.GetValue<bool>("Server:Enabled", false),
        builder.Configuration["Server:BaseUrl"], builder.Configuration["Server:ClientKey"]));
        builder.Services.AddSingleton<ServerClient>(_ => server);
        IHost? host = null;
        try
        {
            StartupSettings? remote = null;
            if (server.Enabled)
            {
                remote = await server.GetSettingsAsync(cancellationToken);

            }
            builder.Services.AddSingleton(new ReportingSettings(
            remote?.ReportSeconds ?? builder.Configuration.GetValue<int>("Reporting:IntervalSeconds", 300),
            deviceType));
            builder.Services.AddSingleton<ReportInterval>();
            builder.Services.AddSingleton(new ActivityHistoryWriter(
            builder.Configuration["History:Directory"] ?? Path.Combine(AppContext.BaseDirectory, "Reports")));
            builder.Services.AddSingleton(new TextReportWriter(Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EcoWattSense", "Reports")));
            builder.Services.AddSingleton(new JsonReportWriter(Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EcoWattSense", "Reports")));
            // Read and validate local thresholds at startup, before starting any monitoring.
            builder.Services.AddSingleton(new StateClassifier(
            remote?.IdleSeconds ?? builder.Configuration.GetValue<double>("StateClassification:IdleThresholdSeconds", 600),
            remote?.UnusedSeconds ?? builder.Configuration.GetValue<double>("StateClassification:UnusedThresholdSeconds", 3600)));
            var identityResult = new DeviceIdentityStore().LoadOrCreate(identityDirectory);
            builder.Services.AddSingleton(identityResult.Identity);
            builder.Services.AddSingleton<WindowsActivityMonitor>();
            builder.Services.AddSingleton<CpuMonitor>();
            builder.Services.AddSingleton<WindowsSessionMonitor>();
            builder.Services.AddSingleton<WindowsPowerMonitor>();
            builder.Services.AddSingleton<Worker>();
            builder.Services.AddHostedService<Worker>(services => services.GetRequiredService<Worker>());

            cancellationToken.ThrowIfCancellationRequested();
            host = builder.Build();
            host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("ActivityHistory")
            .LogInformation("Every activity check is saved to {HistoryPath} (JSON array, retained after shutdown and restart).",
            host.Services.GetRequiredService<ActivityHistoryWriter>().FilePath);
            var reportingSettings = host.Services.GetRequiredService<ReportingSettings>();
            var classifier = host.Services.GetRequiredService<StateClassifier>();
            host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("JsonReports")
            .LogInformation("JSON report: {ReportPath}; completed intervals are retained in the adjacent History folder.",
            host.Services.GetRequiredService<JsonReportWriter>().LatestPath);
            host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("TextReport")
            .LogInformation("Readable report file: {ReportPath}. Saved after each complete valid interval (approximately {IntervalSeconds} seconds). Stopping early does not save a report.",
            host.Services.GetRequiredService<TextReportWriter>().FilePath, reportingSettings.IntervalSeconds);
            string settingsSource = remote is null ? "local configuration"
            : remote.FallbackReason is null ? "server" : "fallback defaults";
            host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("StartupSettings")
            .LogInformation("Effective settings: reporting interval {ReportIntervalSeconds} seconds; idle threshold {IdleThresholdSeconds} seconds; unused threshold {UnusedThresholdSeconds} seconds; source: {SettingsSource}",
            reportingSettings.IntervalSeconds, classifier.IdleThresholdSeconds,
            classifier.UnusedThresholdSeconds, settingsSource);
            host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("DeviceIdentity")
            .LogInformation("Device identity {DeviceId} {IdentityAction} from {IdentityDirectory}",
            identityResult.Identity.Id, identityResult.Created ? "created" : "loaded", identityDirectory);
            if (remote?.FallbackReason is { } reason)
                host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("StartupSettings")
                .LogWarning("Startup settings unavailable ({Reason}); using local defaults 300/600/3600. No retry.", reason);
            return host;
        }
        catch { host?.Dispose(); server.Dispose(); throw; }

    }
}
