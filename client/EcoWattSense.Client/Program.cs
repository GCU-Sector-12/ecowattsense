using EcoWattSense.Client;
using EcoWattSense.Client.Monitoring;

var builder = Host.CreateApplicationBuilder(args);
builder.Services.AddSingleton<WindowsActivityMonitor>();
builder.Services.AddSingleton<CpuMonitor>();
builder.Services.AddHostedService<Worker>();

var host = builder.Build();
host.Run();
