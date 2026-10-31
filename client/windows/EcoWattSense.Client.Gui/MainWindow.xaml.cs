using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using EcoWattSense.Client.Monitoring;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow : Window
{
    private readonly CancellationTokenSource shutdown = new();
    private IHost? host;
    private Worker? worker;
    private Task startup = Task.CompletedTask;
    private bool closing;
    private bool stopped;
    private bool started;
    private bool failed;

    public MainWindow()
    {
        InitializeComponent();
        Loaded += (_, _) =>
        {
            if (closing || started) return;
            started = true;
            startup = StartMonitoringAsync();
        };
        Closing += OnClosing;
    }

    private async Task StartMonitoringAsync()
    {
        try
        {
            ReportStatusValue.Text = "Starting monitoring…";
            var builder = Host.CreateApplicationBuilder(new HostApplicationBuilderSettings
            {
                Args = Environment.GetCommandLineArgs().Skip(1).ToArray(),
                ContentRootPath = AppContext.BaseDirectory
            });
            host = await MonitoringHost.CreateAsync(builder, SelectDeviceType, shutdown.Token);
            shutdown.Token.ThrowIfCancellationRequested();
            worker = host.Services.GetRequiredService<Worker>();
            worker.ReadingAvailable += OnReading;
            await host.StartAsync(shutdown.Token);
            // .NET 10 starts BackgroundService asynchronously: StartAsync alone is not readiness.
            var execution = worker.ExecuteTask ?? throw new InvalidOperationException("Monitoring did not start.");
            await Task.WhenAny(worker.FirstReading, execution).WaitAsync(shutdown.Token);
            if (execution.IsCompleted)
            {
                await execution;
                throw new InvalidOperationException("Monitoring stopped before startup completed.");
            }
            shutdown.Token.ThrowIfCancellationRequested();
            ReportStatusValue.Text = "Monitoring is running. Report status fields are not yet connected.";
            _ = ObserveCompletionAsync(execution);
        }
        catch (OperationCanceledException) when (shutdown.IsCancellationRequested) { }
        catch (Exception exception)
        {
            if (!closing) ShowFailure("Monitoring could not start", exception);
            await StopHostAsync();
        }
    }

    private async Task ObserveCompletionAsync(Task execution)
    {
        try
        {
            await execution;
            if (!closing) ShowFailure("Monitoring stopped", new InvalidOperationException("The worker exited."));
        }
        catch (Exception exception)
        {
            if (!closing) ShowFailure("Monitoring stopped", exception);
        }
    }

    private void ShowFailure(string title, Exception exception)
    {
        failed = true;
        ReportStatusValue.Text = title + ": " + exception.Message;
        ClearReadings();
        MessageBox.Show(this, exception.Message, title, MessageBoxButton.OK, MessageBoxImage.Error);
    }

    private void OnReading(MonitoringReading reading)
    {
        // Queue updates without blocking the worker on the UI thread.
        Dispatcher.BeginInvoke(new Action(() =>
        {
            if (closing || failed) return;
            ActivityValue.Text = reading.State?.ToString() ?? "Unavailable";
            CpuValue.Text = reading.CpuPercentage is { } cpu ? $"{cpu:F1}%" : "Unavailable";
            CpuBar.Value = reading.CpuPercentage ?? 0;
            CpuBar.Visibility = reading.CpuPercentage.HasValue ? Visibility.Visible : Visibility.Hidden;
            InactivityValue.Text = reading.InactivitySeconds is { } seconds ? $"{seconds:F0} seconds" : "Unavailable";
            PowerValue.Text = reading.IsOnBattery switch { true => "Battery", false => "Mains", null => "Unavailable" };
            ScreenValue.Text = "Screen lock: " + (reading.IsLocked switch { true => "locked", false => "unlocked", null => "unavailable" });
            UpdatedValue.Text = $"Last reading: {reading.Timestamp.ToLocalTime():HH:mm:ss}";
        }));
    }

    private void ClearReadings()
    {
        ActivityValue.Text = CpuValue.Text = InactivityValue.Text = PowerValue.Text = "Unavailable";
        ScreenValue.Text = "Screen lock: unavailable";
        CpuBar.Visibility = Visibility.Hidden;
        UpdatedValue.Text = "No live monitoring data";
    }

    private string SelectDeviceType()
    {
        var dialog = new Window
        {
            Title = "Choose device type", Owner = this, Width = 360, Height = 190,
            ResizeMode = ResizeMode.NoResize, WindowStartupLocation = WindowStartupLocation.CenterOwner
        };
        var panel = new StackPanel { Margin = new Thickness(20) };
        panel.Children.Add(new TextBlock { Text = "What type of computer is this?", Margin = new Thickness(0, 0, 0, 16) });
        string? selected = null;
        foreach (var type in new[] { "Laptop", "Desktop" })
        {
            var button = new Button { Content = type, Margin = new Thickness(0, 0, 0, 8) };
            button.Click += (_, _) => { selected = type.ToLowerInvariant(); dialog.DialogResult = true; };
            panel.Children.Add(button);
        }
        dialog.Content = panel;
        if (dialog.ShowDialog() != true || selected is null)
            throw new InvalidOperationException("Device type selection was cancelled. Monitoring has not started.");
        return selected;
    }

    private async Task StopHostAsync()
    {
        if (worker is not null) worker.ReadingAvailable -= OnReading;
        if (host is null) return;
        try
        {
            await host.StopAsync();
            // Do not dispose resources while the worker is still using them.
            if (worker?.ExecuteTask is { } execution)
                try { await execution; } catch (Exception) { /* Already surfaced by startup/completion. */ }
        }
        finally { host.Dispose(); host = null; }
    }

    private async void OnClosing(object? sender, CancelEventArgs e)
    {
        if (stopped) return;
        e.Cancel = true;
        if (closing) return;
        closing = true;
        ReportStatusValue.Text = "Stopping monitoring…";
        shutdown.Cancel();
        try { await startup; await StopHostAsync(); }
        catch (Exception exception) { MessageBox.Show(this, exception.Message, "Monitoring shutdown failed"); }
        finally { stopped = true; shutdown.Dispose(); Close(); }
    }
}
