using EcoWattSense.Client;
using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Monitoring;

var builder = Host.CreateApplicationBuilder(args);
// Read and validate local thresholds at startup, before starting any monitoring.
builder.Services.AddSingleton(new StateClassifier(
    builder.Configuration.GetValue<double>("StateClassification:IdleThresholdSeconds", 600),
    builder.Configuration.GetValue<double>("StateClassification:UnusedThresholdSeconds", 3600)));
var identityDirectory = DeviceIdentityStore.ResolveDirectory(builder.Configuration["DeviceIdentity:Directory"]);
var identityResult = new DeviceIdentityStore().LoadOrCreate(identityDirectory);
builder.Services.AddSingleton(identityResult.Identity);
builder.Services.AddSingleton<WindowsActivityMonitor>();
builder.Services.AddSingleton<CpuMonitor>();
builder.Services.AddSingleton<WindowsSessionMonitor>();
builder.Services.AddSingleton<WindowsPowerMonitor>();
builder.Services.AddHostedService<Worker>();

using var host = builder.Build();
host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("DeviceIdentity")
    .LogInformation("Device identity {DeviceId} {IdentityAction} from {IdentityDirectory}",
        identityResult.Identity.Id, identityResult.Created ? "created" : "loaded", identityDirectory);
host.Run();
