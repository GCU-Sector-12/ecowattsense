// MainWindow.Viewer.cs: the viewer-mode half of MainWindow (SPEC section 9).
// Who uses it: App.xaml.cs calls StartViewer when the EcoWattSense service is installed; the history agent calls
// ShowHistorySummary and puts its chart control into the Border named HistoryHost.
// What it does: swaps the classic cards for the viewer cards, starts the pipe reporter, and refreshes the cards from
// status.json on a file event while the window is visible (plus once at start and on each show). It never samples,
// never reports and never takes instance.lock.
// It lives in its own file so MainWindow.xaml.cs keeps its shape.
#nullable enable
using System.Windows;
using System.Windows.Threading;
using EcoWattSense.Client.Gui.Viewer;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow
{
    private ServicePipeReporter? pipeReporter;
    private StatusFolderWatcher? statusWatcher;

    /// <summary>Switches the window to viewer mode.</summary>
    internal void StartViewer()
    {
        LivePanel.Visibility = Visibility.Collapsed;
        ViewerPanel.Visibility = Visibility.Visible;
        ShowHistorySummary(null, null);
        // There is no activation pipe in viewer mode, so the tray icon is what brings a hidden window back.
        trayIcon.Visible = true;
        statusWatcher = new StatusFolderWatcher(ViewerSources.StatusFolder, "status.json");
        statusWatcher.Changed += OnStatusChanged;
        pipeReporter = new ServicePipeReporter(new ServicePipeSender(), ViewerSources.ReadIdleSeconds, ViewerSources.ReadBrightness);
        pipeReporter.Start();
        IsVisibleChanged += OnViewerVisibleChanged;
        Closed += OnViewerClosed;
        RefreshViewer();
        statusWatcher.SetActive(IsVisible);
    }

    /// <summary>Fills the Power now and Today cards from the history, which another part of the app reads.</summary>
    /// <param name="powerNowWatts">The device power of the last history row in watts, or null when unknown.</param>
    /// <param name="todayKwh">The energy of today in kilowatt hours, or null when unknown.</param>
    /// <remarks>May be called from any thread.</remarks>
    public void ShowHistorySummary(double? powerNowWatts, double? todayKwh)
    {
        if (!Dispatcher.CheckAccess())
        {
            Dispatcher.BeginInvoke(new Action<double?, double?>(ShowHistorySummary), DispatcherPriority.Normal, powerNowWatts, todayKwh);
            return;
        }
        PowerNowValue.Text = ViewerText.Watts(powerNowWatts);
        TodayValue.Text = ViewerText.Kilowatthours(todayKwh);
    }

    /// <summary>Reads status.json and the other card sources and fills the cards.</summary>
    private void RefreshViewer()
    {
        StatusInfo status = StatusReader.Read(ViewerSources.StatusFile);
        TimeZoneInfo zone = TimeZoneInfo.Local;
        ServiceValue.Text = ViewerMode.Word(ScmServiceLookup.Query());
        LevelValue.Text = ViewerText.LevelWord(status.Level);
        LevelLineValue.Text = ViewerText.LevelLine(status.Level);
        PowerSourceViewerValue.Text = ViewerText.PowerSource(ViewerSources.ReadOnBattery());
        LastReportValue.Text = ViewerText.LastReport(status, zone);
        CalibratedValue.Text = ViewerText.Calibrated(status, zone);
        DeviceIdValue.Text = ViewerText.ShortId(ViewerSources.ReadDeviceIdText());
    }

    /// <summary>Queues a refresh on the window thread when the status file changes.</summary>
    private void OnStatusChanged()
        => Dispatcher.BeginInvoke(new Action(RefreshViewer), DispatcherPriority.Background);

    /// <summary>Switches the file events on or off with the visibility of the window, and reads once on show.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">The visibility change.</param>
    private void OnViewerVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
    {
        bool visible = IsVisible;
        statusWatcher?.SetActive(visible);
        if (visible) RefreshViewer();
    }

    /// <summary>Stops the pipe reporter and the file watcher when the window closes.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnViewerClosed(object? sender, EventArgs e)
    {
        pipeReporter?.Dispose();
        statusWatcher?.Dispose();
    }
}
