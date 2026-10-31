using EcoWattSense.Client;
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
catch (OperationCanceledException) when (startupCancellation.IsCancellationRequested) { return; }
finally { Console.CancelKeyPress -= cancel; }
using (host) { await host.RunAsync(); }
