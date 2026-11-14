// MainWindow.Viewer.cs: the viewer-mode half of MainWindow (SPEC section 9).
// Who uses it: App.xaml.cs calls StartViewer when the EcoWattSense service is installed.
// It also shows the 24 hour chart in the Border named HistoryHost and fills the history cards, and refreshes them
// when a history file changes (one coalesced refresh per burst of file events).
// What it does: swaps the classic cards for the viewer cards, starts the pipe reporter, and refreshes the cards from
// status.json on a file event while the window is visible (plus once at start and on each show). It never samples,
// never reports and never takes instance.lock.
// It lives in its own file so MainWindow.xaml.cs keeps its shape.
#nullable enable
using System.IO;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;
using EcoWattSense.Client.Gui.History;
using EcoWattSense.Client.Gui.Settings;
using EcoWattSense.Client.Gui.Viewer;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow
{
    private ServicePipeReporter? pipeReporter;
    private StatusFolderWatcher? statusWatcher;
    private StatusFolderWatcher? historyWatcher;
    private RefreshCoalescer? historyRefresh;
    private HistoryChart? historyChart;
    private DonutChart? donutChart;

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
        StartHistory();
        StartSettings();
        StartCalibrate();
        IsVisibleChanged += OnViewerVisibleChanged;
        Closed += OnViewerClosed;
        RefreshViewer();
        statusWatcher.SetActive(IsVisible);
        historyWatcher?.SetActive(IsVisible);
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
        ServiceState serviceState = ScmServiceLookup.Query();
        ServiceValue.Text = ViewerMode.Word(serviceState);
        ShowServiceDot(serviceState);
        LevelValue.Text = ViewerText.LevelWord(status.Level);
        LevelLineValue.Text = ViewerText.LevelLine(status.Level);
        PowerSourceViewerValue.Text = ViewerText.PowerSource(ViewerSources.ReadOnBattery());
        LastReportValue.Text = ViewerText.LastReport(status, zone);
        CalibratedValue.Text = ViewerText.Calibrated(status, zone);
        DeviceIdValue.Text = ViewerText.ShortId(ViewerSources.ReadDeviceIdText());
        ShowSettings(SettingsReader.Read(ViewerSources.StatusFile));
        UpdateCalibrateButton();
    }

    /// <summary>Puts the 24 hour chart into HistoryHost, and wires the history file events to one coalesced refresh.</summary>
    private void StartHistory()
    {
        historyChart = new HistoryChart();
        HistoryHost.Child = historyChart;
        donutChart = new DonutChart();
        DonutHost.Child = donutChart;
        DonutLegendHost.Child = donutChart.Legend;
        historyRefresh = new RefreshCoalescer(PostBackground, RefreshHistory);
        historyWatcher = new StatusFolderWatcher(ViewerSources.HistoryFolder, "*.csv");
        historyWatcher.Changed += historyRefresh.Request;
        RefreshHistory();
    }

    /// <summary>Queues an action on the window thread at background priority. The coalescer uses it to wait for the window thread.</summary>
    /// <param name="action">The action to run later.</param>
    private void PostBackground(Action action)
        => Dispatcher.BeginInvoke(action, DispatcherPriority.Background);

    /// <summary>Starts a history read off the window thread, so a slow disk never freezes the window.</summary>
    private void RefreshHistory() => _ = Task.Run(ReadHistory);

    /// <summary>Reads the last 24 hours and hands the rows to the window thread.</summary>
    /// <remarks>Runs on a pool thread. A missing folder gives no rows. A file problem the reader does not absorb gives no rows too.</remarks>
    private void ReadHistory()
    {
        DateTime nowUtc = DateTime.UtcNow;
        HistoryReadResult result = ReadHistoryOrEmpty(nowUtc);
        Dispatcher.BeginInvoke(new Action<HistoryReadResult, DateTime>(ShowHistory), DispatcherPriority.Normal, result, nowUtc);
    }

    /// <summary>Reads the history and turns a file system failure into an empty result.</summary>
    /// <param name="nowUtc">The current time, Kind Utc.</param>
    /// <returns>The rows, or an empty result when the read failed.</returns>
    private static HistoryReadResult ReadHistoryOrEmpty(DateTime nowUtc)
    {
        try { return HistoryReader.Read(ViewerSources.HistoryFolder, nowUtc); }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return new HistoryReadResult();
        }
    }

    /// <summary>Draws the chart and fills the two history cards. Runs on the window thread.</summary>
    /// <param name="result">The rows of the last 24 hours.</param>
    /// <param name="nowUtc">The time the rows were read at.</param>
    private void ShowHistory(HistoryReadResult result, DateTime nowUtc)
    {
        historyChart?.SetRows(result.Rows, nowUtc);
        donutChart?.SetRows(result.Rows, nowUtc);
        ShowLastMeasurement(result.Rows);
        ShowHistorySummary(
            HistorySummary.PowerNow(result.Rows, nowUtc),
            HistorySummary.EnergyTodayKwh(result.Rows, nowUtc, TimeZoneInfo.Local));
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
        historyWatcher?.SetActive(visible);
        if (visible) RefreshViewer();
        if (visible) RefreshHistory();
    }

    /// <summary>Stops the pipe reporter and the file watcher when the window closes.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnViewerClosed(object? sender, EventArgs e)
    {
        StopCalibrate();
        pipeReporter?.Dispose();
        statusWatcher?.Dispose();
        historyWatcher?.Dispose();
    }
}
