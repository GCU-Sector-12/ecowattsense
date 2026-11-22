// MainWindow.Developer.cs: the Developer page of the viewer mode (spec D2, sections 2.1 to 2.3).
// Who uses it: StartViewer and OnViewerClosed (MainWindow.Viewer.cs), ShowPage (MainWindow.Settings.cs), and the click
// handlers in MainWindow.xaml.
// What it does: the "Developer mode" check box (About card of the Settings page) switches the sidebar item "Developer"
// on or off. The page shows six tabs of raw data that the service writes: Readings, Reports, Sends and events, Sources,
// Status, Log. The data is read off the window thread (DiagSnapshotLoader) when a file in <data>\logs\, status\ or scan\
// changes, and only while the page is visible. There is no timer. "Export diagnostics..." writes a ZIP (DiagExporter)
// off the window thread. All logic is in the Developer\Logic folder, which has no WPF in it.
// A tab shows what was read at the last refresh. The grids are rebuilt on each refresh, so a table scrolls to the newest
// row when new data arrives. The watchers are made when the page is shown, so a folder that does not exist yet is picked
// up the next time the page is shown (the same rule as the other watchers of the viewer).
#nullable enable
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Threading;
using EcoWattSense.Client.Gui.Developer;
using EcoWattSense.Client.Gui.Viewer;
using Brushes = System.Windows.Media.Brushes;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow
{
    private const double DeveloperChromeHeight = 250;
    private const double DeveloperMinTabsHeight = 260;
    private const double CharWidth = 7.5;
    private const double CellPadding = 14;

    private StatusFolderWatcher? developerLogsWatcher;
    private StatusFolderWatcher? developerStatusWatcher;
    private StatusFolderWatcher? developerScanWatcher;
    private RefreshCoalescer? developerRefresh;
    private int developerReadId;
    private bool developerPageShown;

    /// <summary>Makes the watchers and reads the developer mode file. Called once when the viewer starts.</summary>
    internal void StartDeveloper()
    {
        var paths = new DiagPaths(ViewerSources.DataFolder);
        developerRefresh = new RefreshCoalescer(PostBackground, RefreshDeveloper);
        developerLogsWatcher = new StatusFolderWatcher(paths.LogsFolder, "*");
        developerStatusWatcher = new StatusFolderWatcher(paths.StatusFolder, "*.json");
        developerScanWatcher = new StatusFolderWatcher(paths.ScanFolder, "*.json");
        developerLogsWatcher.Changed += developerRefresh.Request;
        developerStatusWatcher.Changed += developerRefresh.Request;
        developerScanWatcher.Changed += developerRefresh.Request;
        SizeChanged += OnDeveloperWindowSizeChanged;
        _ = ApplyDeveloperModeAsync();
    }

    /// <summary>Releases the watchers when the window closes.</summary>
    private void StopDeveloper()
    {
        developerLogsWatcher?.Dispose();
        developerStatusWatcher?.Dispose();
        developerScanWatcher?.Dispose();
    }

    /// <summary>Reads the developer mode file off the window thread and sets the check box and the sidebar item.</summary>
    /// <returns>A task that ends when the check box is set.</returns>
    private async Task ApplyDeveloperModeAsync()
    {
        bool on = await Task.Run(() => DeveloperModeFile.IsOn(DeveloperModeFile.DefaultPath));
        DeveloperCheck.IsChecked = on;
        DeveloperCheck.IsEnabled = true;
        ShowDeveloperNav(on);
    }

    /// <summary>Writes or removes the developer mode file when the user clicks the check box.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private async void OnDeveloperModeClick(object sender, RoutedEventArgs e)
    {
        bool wanted = DeveloperCheck.IsChecked == true;
        DeveloperCheck.IsEnabled = false;
        bool saved = await Task.Run(() => DeveloperModeFile.TrySet(DeveloperModeFile.DefaultPath, wanted));
        bool now = saved ? wanted : !wanted;
        DeveloperCheck.IsChecked = now;
        DeveloperNote.Text = saved ? "" : DiagText.ModeNotSaved;
        DeveloperCheck.IsEnabled = true;
        ShowDeveloperNav(now);
    }

    /// <summary>Shows or hides the sidebar item. When the page is open and the item goes away, the first page comes back.</summary>
    /// <param name="on">True when developer mode is on.</param>
    private void ShowDeveloperNav(bool on)
    {
        DeveloperNav.Visibility = on ? Visibility.Visible : Visibility.Collapsed;
        if (!on && developerPageShown) ShowPage(false);
    }

    /// <summary>Shows the Developer page.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnDeveloperNavClick(object sender, RoutedEventArgs e) => ShowDeveloperPage();

    /// <summary>Switches the right side to the Developer page. Does nothing when developer mode is off.</summary>
    private void ShowDeveloperPage()
    {
        if (DeveloperNav.Visibility != Visibility.Visible) return;
        settingsPageShown = false;
        developerPageShown = true;
        ViewerPanel.Visibility = Visibility.Collapsed;
        SettingsPanel.Visibility = Visibility.Collapsed;
        PageTitle.Text = "Developer";
        PageSubtitle.Text = "Raw readings and reports of the service. Newest first.";
        ComputerNav.Background = Brushes.Transparent;
        SettingsNav.Background = Brushes.Transparent;
        DeveloperNav.Background = SelectedNavBrush;
        ExportHost.Visibility = Visibility.Visible;
        FitDeveloperTabs();
        DeveloperPanel.Visibility = Visibility.Visible;
    }

    /// <summary>Hides the Developer page and its export button. ShowPage calls it before it shows another page.</summary>
    private void HideDeveloperPage()
    {
        developerPageShown = false;
        DeveloperPanel.Visibility = Visibility.Collapsed;
        ExportHost.Visibility = Visibility.Collapsed;
        DeveloperNav.Background = Brushes.Transparent;
    }

    /// <summary>Gives the tabs the height of the window, so the tables scroll inside the page and not the page.</summary>
    private void FitDeveloperTabs() => DeveloperTabs.Height = Math.Max(DeveloperMinTabsHeight, ActualHeight - DeveloperChromeHeight);

    /// <summary>Fits the tabs when the window size changes.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnDeveloperWindowSizeChanged(object sender, SizeChangedEventArgs e) => FitDeveloperTabs();

    /// <summary>Switches the file events on or off with the visibility of the page, and reads once on show.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnDeveloperVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
    {
        bool visible = DeveloperPanel.IsVisible;
        developerLogsWatcher?.SetActive(visible);
        developerStatusWatcher?.SetActive(visible);
        developerScanWatcher?.SetActive(visible);
        if (visible) developerRefresh?.Request();
    }

    /// <summary>Starts a read off the window thread. A result that comes after a newer read was started is dropped.</summary>
    private void RefreshDeveloper()
    {
        int id = Interlocked.Increment(ref developerReadId);
        _ = Task.Run(() => LoadDeveloper(id));
    }

    /// <summary>Reads all the data of the page and hands it to the window thread.</summary>
    /// <param name="id">The number of this read.</param>
    /// <remarks>Runs on a pool thread. The loader turns file problems into "unknown" results.</remarks>
    private void LoadDeveloper(int id)
    {
        DiagSnapshot snapshot = DiagSnapshotLoader.Load(ViewerSources.DataFolder, DateTimeOffset.UtcNow);
        Dispatcher.BeginInvoke(new Action<DiagSnapshot, int>(ShowDeveloper), DispatcherPriority.Normal, snapshot, id);
    }

    /// <summary>Fills the tabs. Runs on the window thread.</summary>
    /// <param name="snapshot">What was read.</param>
    /// <param name="id">The number of the read. An old number is ignored.</param>
    private void ShowDeveloper(DiagSnapshot snapshot, int id)
    {
        if (id != Volatile.Read(ref developerReadId)) return;
        DiagReadResult counts = snapshot.Counts;
        bool none = counts.TotalLines == 0;
        FillTable(ReadingsGrid, snapshot.Readings);
        FillTable(ReportsGrid, snapshot.Reports);
        FillTable(SendsGrid, snapshot.SendsEvents);
        ReadingsLine.Text = DiagText.CountLine(snapshot.Readings.Rows.Count, counts.SkippedLines, none, counts.FolderUnreadable);
        ReportsLine.Text = DiagText.CountLine(snapshot.Reports.Rows.Count, counts.SkippedLines, none, counts.FolderUnreadable);
        SendsLine.Text = DiagText.CountLine(snapshot.SendsEvents.Rows.Count, counts.SkippedLines, none, counts.FolderUnreadable);
        ShowFileRows(SourcesServiceGrid, SourcesServiceLine, "capabilities.service.json", snapshot.CapabilitiesService);
        ShowFileRows(SourcesGrid, SourcesLine, "capabilities.json", snapshot.Capabilities);
        ShowFileRows(StatusGrid, StatusLine, "status.json", snapshot.Status);
        LogBox.Text = string.Join("\n", snapshot.Log.Lines);
        LogLine.Text = DiagText.LogLine(snapshot.Log.Available, snapshot.Log.Lines.Count, snapshot.Log.SkippedLines);
    }

    /// <summary>Shows one "name, value" file in a grid and its line.</summary>
    /// <param name="grid">The grid with the two fixed columns.</param>
    /// <param name="line">The line above the grid.</param>
    /// <param name="fileName">The name of the file, for the line.</param>
    /// <param name="result">The rows of the file.</param>
    private static void ShowFileRows(DataGrid grid, TextBlock line, string fileName, JsonFileResult result)
    {
        grid.ItemsSource = result.Rows;
        line.Text = fileName + ": " + DiagText.FileLine(result.Available, result.Rows.Count);
    }

    /// <summary>Rebuilds the columns of a grid and sets its rows.</summary>
    /// <param name="grid">The grid.</param>
    /// <param name="table">The table: the columns and the rows, newest first.</param>
    private static void FillTable(DataGrid grid, DiagTable table)
    {
        grid.ItemsSource = null;
        grid.Columns.Clear();
        for (int i = 0; i < table.Columns.Count; i++) grid.Columns.Add(MakeColumn(table, i));
        grid.ItemsSource = table.Rows;
    }

    /// <summary>Makes one column that shows the text at one position of the row array.</summary>
    /// <param name="table">The table.</param>
    /// <param name="index">The column position.</param>
    /// <returns>The column. The header is a TextBlock, so an underscore in a name is not taken as an access key.</returns>
    private static DataGridTextColumn MakeColumn(DiagTable table, int index) => new()
    {
        Header = new TextBlock { Text = table.Columns[index] },
        Binding = new Binding("[" + index + "]") { Mode = BindingMode.OneTime },
        IsReadOnly = true,
        Width = new DataGridLength(table.WidthChars[index] * CharWidth + CellPadding)
    };

    /// <summary>Asks for a file name, then writes the diagnostics ZIP off the window thread. The button is off while it runs.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private async void OnExportClick(object sender, RoutedEventArgs e)
    {
        var dialog = new Microsoft.Win32.SaveFileDialog
        {
            FileName = DiagText.ExportFileName(DateTime.Now),
            DefaultExt = ".zip",
            AddExtension = true,
            Filter = "ZIP file (*.zip)|*.zip",
            OverwritePrompt = true
        };
        if (dialog.ShowDialog(this) != true) return;
        string target = dialog.FileName;
        ExportButton.IsEnabled = false;
        ExportNote.Text = "";
        ExportResult result = await Task.Run(() => RunExport(target));
        ExportNote.Text = result.Saved ? DiagText.ExportSaved : DiagText.ExportFailed;
        ExportButton.IsEnabled = true;
    }

    /// <summary>Runs the export on a pool thread. Any unexpected error becomes a failed result, because a background job must not close the viewer.</summary>
    /// <param name="target">The ZIP path the user chose.</param>
    /// <returns>The result. Saved is false when the export failed.</returns>
    private static ExportResult RunExport(string target)
    {
        try { return DiagExporter.Export(ViewerSources.DataFolder, target); }
        catch (Exception) { return new ExportResult(false, 0, 0); }
    }
}
