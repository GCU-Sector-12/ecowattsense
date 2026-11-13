// Defines the monitoring window and tray controls used by the Windows GUI client.

using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Threading;
using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Monitoring;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Forms = System.Windows.Forms;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow : Window
{
    private readonly CancellationTokenSource shutdown = new();
    private readonly Forms.ContextMenuStrip trayMenu = new();
    private readonly Forms.NotifyIcon trayIcon;
    private GuiActivationPipe? activation;
    private IHost? host;
    private Worker? worker;
    private Task startup = Task.CompletedTask;
    private bool closing;
    private bool allowClose;
    private bool failed;

    /// <summary>Creates the monitoring window, tray menu and window lifetime handlers.</summary>
    public MainWindow()
    {
        InitializeComponent();
        trayMenu.Items.Add("Open", null, (_, _) => RestoreWindow());
        trayMenu.Items.Add("Exit", null, async (_, _) => await ExitAsync());
        trayIcon = new Forms.NotifyIcon
        {
            Text = "EcoWattSense",
            Icon = System.Drawing.SystemIcons.Application,
            ContextMenuStrip = trayMenu
        };
        Closing += (_, e) =>
        {
            if (allowClose) return;
            e.Cancel = true;
            Hide();
        };
        Closed += (_, _) =>
        {
            trayIcon.Dispose();
            trayMenu.Dispose();
            shutdown.Dispose();
        };
    }

    /// <summary>Shows the main window and activates visible owned dialogs.</summary>
    /// <returns>False if the window is closing. Otherwise, true.</returns>
    private bool RestoreWindow()
    {
        if (closing || allowClose) return false;
        Show();
        if (WindowState == WindowState.Minimized) WindowState = WindowState.Normal;
        Activate();
        foreach (Window dialog in OwnedWindows)
            ActivateVisibleDialog(dialog);
        return true;
    }

    /// <summary>Activates an owned dialog when visible.</summary>
    /// <param name="dialog">The owned window to check.</param>
    private static void ActivateVisibleDialog(Window dialog)
    {
        if (dialog.IsVisible) dialog.Activate();
    }

    /// <summary>Starts the activation listener and shows the tray icon.</summary>
    /// <param name="lockPath">The path to the instance lock.</param>
    /// <returns>A task that completes when the listener is available.</returns>
    private async Task StartActivationAsync(string lockPath)
    {
        activation = await Task.Run(() => new GuiActivationPipe(lockPath,
            token => Dispatcher.InvokeAsync(RestoreWindow, DispatcherPriority.Normal, token).Task));
        trayIcon.Visible = !closing;
    }

    /// <summary>Releases the activation listener during shutdown.</summary>
    /// <returns>A task that completes when the listener stops.</returns>
    private async Task StopActivationAsync()
    {
        if (activation is null) return;
        var listener = activation;
        activation = null;
        await listener.DisposeAsync();
    }

    /// <summary>Records the startup task so tray shutdown can wait for it.</summary>
    /// <param name="hostArgs">Command-line arguments for the monitoring host.</param>
    /// <param name="startHidden">Whether the launch requested tray-only startup.</param>
    internal void StartMonitoring(string[] hostArgs, bool startHidden)
        => startup = StartMonitoringAsync(hostArgs, startHidden);

    /// <summary>Starts monitoring and handles duplicate launches and startup errors.</summary>
    /// <param name="hostArgs">Command-line arguments for the monitoring host.</param>
    /// <param name="startHidden">Whether the launch requested tray-only startup.</param>
    /// <returns>A task that completes after startup or error handling.</returns>
    private async Task StartMonitoringAsync(string[] hostArgs, bool startHidden)
    {
        // Record the startup task before a modal prompt can process tray commands.
        await Task.Yield();
        try
        {
            await StartHostAsync(hostArgs);
        }
        catch (OperationCanceledException) when (shutdown.IsCancellationRequested) { }
        catch (ClientAlreadyRunningException duplicate)
        {
            await HandleDuplicateLaunchAsync(duplicate.LockPath, startHidden);
        }
        catch (Exception exception)
        {
            await HandleStartupFailureAsync(exception);
        }
    }

    /// <summary>Starts the monitoring host and waits for the first reading.</summary>
    /// <param name="hostArgs">Command-line arguments for the monitoring host.</param>
    /// <returns>A task that completes when monitoring is ready.</returns>
    /// <exception cref="ClientAlreadyRunningException">Another instance holds the monitoring lock.</exception>
    /// <exception cref="InvalidOperationException">
    /// Device type selection is cancelled or invalid, or the server settings request is rejected or returns invalid data.
    /// The worker may also fail to start or exit before the first reading.
    /// </exception>
    /// <exception cref="ArgumentOutOfRangeException">The configured activity thresholds are invalid.</exception>
    /// <exception cref="OperationCanceledException">The window is shutting down.</exception>
    private async Task StartHostAsync(string[] hostArgs)
    {
        ReportStatusValue.Text = "Starting monitoring…";
        var builder = Host.CreateApplicationBuilder(new HostApplicationBuilderSettings
        {
            Args = hostArgs,
            ContentRootPath = AppContext.BaseDirectory
        });
        host = await MonitoringHost.CreateAsync(builder, SelectDeviceType, shutdown.Token, StartActivationAsync);
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

    /// <summary>Closes the duplicate and requests activation for a visible launch.</summary>
    /// <param name="lockPath">The path to the instance lock.</param>
    /// <param name="startHidden">Whether the launch requested tray-only startup.</param>
    /// <returns>A task that completes when the duplicate closes or shutdown takes over.</returns>
    private async Task HandleDuplicateLaunchAsync(string lockPath, bool startHidden)
    {
        if (startHidden)
        {
            allowClose = true;
            Close();
            return;
        }
        bool opened = await Task.Run(() => GuiActivationPipe.RequestAsync(lockPath, shutdown.Token));
        if (closing) return;
        if (!opened)
        {
            Show();
            MessageBox.Show(this,
                "EcoWattSense is already running, but its window could not be opened in this Windows session. " +
                "It may be running in the background or in another session. This copy will close.",
                "EcoWattSense", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        allowClose = true;
        await Dispatcher.InvokeAsync(Close);
    }

    /// <summary>Cleans up startup resources and reports the error if the window is open.</summary>
    /// <param name="exception">The monitoring startup error.</param>
    /// <returns>A task that completes after cleanup and error handling.</returns>
    private async Task HandleStartupFailureAsync(Exception exception)
    {
        try { await StopActivationAsync(); }
        finally { await StopHostAsync(); }

        if (closing) return;
        trayIcon.Visible = false;
        try { ShowFailure("Monitoring could not start", exception); }
        finally { allowClose = true; Close(); }
    }

    /// <summary>Watches the worker and reports an unexpected stop while the window is open.</summary>
    /// <param name="execution">The running worker task.</param>
    /// <returns>A task that completes after the worker stops and any failure is reported.</returns>
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

    /// <summary>Displays a monitoring error and clears readings that are no longer live.</summary>
    /// <param name="title">The error dialog title and status label.</param>
    /// <param name="exception">The error to display.</param>
    private void ShowFailure(string title, Exception exception)
    {
        failed = true;
        ReportStatusValue.Text = title + ": " + exception.Message;
        ClearReadings();
        new WindowInteropHelper(this).EnsureHandle();
        MessageBox.Show(this, exception.Message, title, MessageBoxButton.OK, MessageBoxImage.Error);
    }

    /// <summary>Queues a live monitoring reading for display on the GUI thread.</summary>
    /// <param name="reading">The observation from the monitoring worker.</param>
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

    /// <summary>Clears the displayed readings when monitoring fails.</summary>
    private void ClearReadings()
    {
        ActivityValue.Text = CpuValue.Text = InactivityValue.Text = PowerValue.Text = "Unavailable";
        ScreenValue.Text = "Screen lock: unavailable";
        CpuBar.Visibility = Visibility.Hidden;
        UpdatedValue.Text = "No live monitoring data";
    }

    /// <summary>Asks the user to select the computer type for monitoring.</summary>
    /// <returns>The selected device type in lowercase.</returns>
    /// <exception cref="InvalidOperationException">The selection dialog is cancelled.</exception>
    private string SelectDeviceType()
    {
        // WPF needs an owner handle even if the main window has never been shown.
        new WindowInteropHelper(this).EnsureHandle();
        var dialog = new Window
        {
            Title = "Choose device type", Owner = this, Width = 360, Height = 190,
            ResizeMode = ResizeMode.NoResize,
            WindowStartupLocation = IsVisible ? WindowStartupLocation.CenterOwner : WindowStartupLocation.CenterScreen
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

    /// <summary>Stops the host and waits for the worker before disposing host resources.</summary>
    /// <returns>A task that completes after host cleanup.</returns>
    /// <exception cref="Exception">Stopping or disposing the host fails.</exception>
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

    /// <summary>Cancels startup and monitoring, releases resources and closes the GUI.</summary>
    /// <returns>A task that completes after shutdown. Cleanup errors are shown in a dialog.</returns>
    private async Task ExitAsync()
    {
        if (closing) return;
        closing = true;
        trayMenu.Enabled = false;
        ReportStatusValue.Text = "Stopping monitoring…";
        shutdown.Cancel();
        foreach (Window dialog in OwnedWindows) dialog.Close();
        try
        {
            await startup;
            try { await StopActivationAsync(); }
            finally { await StopHostAsync(); }
        }
        catch (Exception exception) { MessageBox.Show(this, exception.Message, "Monitoring shutdown failed"); }
        finally { allowClose = true; Close(); }
    }
}
