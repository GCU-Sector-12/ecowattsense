using EcoWattSense.Client;
using EcoWattSense.Client.Monitoring;

var builder = Host.CreateApplicationBuilder(args);
// Read and validate local thresholds at startup, before starting any monitoring.
builder.Services.AddSingleton(new StateClassifier(
    builder.Configuration.GetValue<double>("StateClassification:IdleThresholdSeconds", 600),
    builder.Configuration.GetValue<double>("StateClassification:UnusedThresholdSeconds", 3600)));
builder.Services.AddSingleton<WindowsActivityMonitor>();
builder.Services.AddSingleton<CpuMonitor>();
builder.Services.AddSingleton<WindowsSessionMonitor>();
builder.Services.AddSingleton<WindowsPowerMonitor>();
builder.Services.AddHostedService<Worker>();

var host = builder.Build();
host.Run();
