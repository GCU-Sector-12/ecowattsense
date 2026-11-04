using EcoWattSense.Client;
using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Reporting;

var builder = Host.CreateApplicationBuilder(args);
using var startupCancellation = new CancellationTokenSource();
ConsoleCancelEventHandler cancel = (_, e) => { e.Cancel = true; startupCancellation.Cancel(); };
Console.CancelKeyPress += cancel;
IHost host;
try
{
    host = await MonitoringHost.CreateAsync(builder,
        () => DeviceTypeSelection.Prompt(Console.In, Console.Out, Environment.UserInteractive),
        startupCancellation.Token);
}
catch (OperationCanceledException) when (startupCancellation.IsCancellationRequested) { return 0; }
catch (ClientAlreadyRunningException alreadyRunning)
{
    // Build a host only for its loggers (nothing starts), so the background exe, which has no
    // window, writes this Warning to the Event Log. The console exe prints it.
    using (var notice = builder.Build())
        notice.Services.GetRequiredService<ILoggerFactory>().CreateLogger("SingleInstance")
            .LogWarning("{Message} Lock file: {LockPath}", alreadyRunning.Message, alreadyRunning.LockPath);
    return ClientAlreadyRunningException.ExitCode;
}
finally { Console.CancelKeyPress -= cancel; }
using (host) { await host.RunAsync(); }
return 0;
