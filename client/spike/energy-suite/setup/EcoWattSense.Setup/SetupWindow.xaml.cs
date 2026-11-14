// SetupWindow.xaml.cs: code behind of the Setup window.
// Who uses it: App creates one window per run. The window only shows pages and collects input; every real action is done
// by InstallEngine and MaintenanceEngine, which run on a worker thread so the window stays responsive.
// Pages: 0 Scan, 1 Server, 2 Install, 3 Calibration, 4 Uninstall. Install mode walks 0 to 3; /settings shows page 1 only;
// /uninstall shows page 4 only. The server key is read once from the password box, passed on and cleared.
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace EcoWattSense.Setup;

/// <summary>The three ways Setup can run.</summary>
public enum SetupMode
{
    /// <summary>The normal install or upgrade.</summary>
    Install,
    /// <summary>`Setup.exe /settings`: the Server page and the device type only.</summary>
    Settings,
    /// <summary>`Setup.exe /uninstall`.</summary>
    Uninstall,
}

/// <summary>The Setup window.</summary>
public partial class SetupWindow : Window
{
    private static readonly SolidColorBrush ActiveBrush = new(Color.FromRgb(0x28, 0x5E, 0x50));
    private static readonly SolidColorBrush ErrorBrush = new(Color.FromRgb(0xB3, 0x26, 0x1E));
    private static readonly SolidColorBrush GoodBrush = new(Color.FromRgb(0x1B, 0x6E, 0x3C));

    private readonly SetupMode _mode;
    private readonly SetupServices _services;
    private readonly SetupLayout _layout;
    private readonly string _packageDir;
    private readonly string? _tempCopyDir;
    private readonly InstallEngine _install;
    private readonly MaintenanceEngine _maintenance;
    private readonly Func<Task>[] _nextActions;
    private readonly string[] _nextTexts = ["Next", "Next", "Install", "Finish", "Uninstall"];
    private StackPanel[] _pages = [];
    private Border[] _stepItems = [];
    private int _page;
    private bool _finished;
    private bool _hadUrl;
    private bool _keyStored;

    /// <summary>Creates the window.</summary>
    /// <param name="mode">Install, settings or uninstall.</param>
    /// <param name="services">The operations.</param>
    /// <param name="layout">The folders.</param>
    /// <param name="packageDir">The folder Setup runs from.</param>
    /// <param name="tempCopyDir">The temporary copy Setup runs from during an uninstall, or null.</param>
    public SetupWindow(SetupMode mode, SetupServices services, SetupLayout layout, string packageDir, string? tempCopyDir)
    {
        InitializeComponent();
        _mode = mode;
        _services = services;
        _layout = layout;
        _packageDir = packageDir;
        _tempCopyDir = tempCopyDir;
        SetupLog log = new(services, layout);
        _install = new InstallEngine(services, layout, log);
        _maintenance = new MaintenanceEngine(services, layout, log);
        _nextActions = [NextScan, mode == SetupMode.Settings ? NextSaveSettings : NextServer, NextInstall, NextClose, NextUninstall];
        _pages = [PageScan, PageServer, PageInstall, PageCalibration, PageUninstall];
        _stepItems = [StepScanItem, StepServerItem, StepInstallItem, StepCalibrationItem];
        LoadServerValues();
        Loaded += Window_Loaded;
    }

    private async void Window_Loaded(object sender, RoutedEventArgs e)
    {
        PrepareForMode();
        if (_mode == SetupMode.Install) await RunPrepare();
    }

    /// <summary>Chooses the first page and the sidebar for the mode.</summary>
    private void PrepareForMode()
    {
        bool install = _mode == SetupMode.Install;
        StepScanItem.Visibility = install ? Visibility.Visible : Visibility.Collapsed;
        StepInstallItem.Visibility = install ? Visibility.Visible : Visibility.Collapsed;
        StepCalibrationItem.Visibility = install ? Visibility.Visible : Visibility.Collapsed;
        StepServerText.Text = _mode == SetupMode.Install ? "2  Server" : _mode == SetupMode.Settings ? "Settings" : "Uninstall";
        DevicePanel.Visibility = _mode == SetupMode.Settings ? Visibility.Visible : Visibility.Collapsed;
        AutostartBox.Visibility = DevicePanel.Visibility;
        ServerHeading.Text = _mode == SetupMode.Settings ? "Settings" : "Server";
        _nextTexts[1] = _mode == SetupMode.Settings ? "Save" : "Next";
        Show(install ? 0 : _mode == SetupMode.Settings ? 1 : 4);
        NextButton.IsEnabled = !install;
    }

    /// <summary>Shows one page, marks it in the sidebar and sets the buttons.</summary>
    private void Show(int page)
    {
        _page = page;
        for (int index = 0; index < _pages.Length; index++) _pages[index].Visibility = index == page ? Visibility.Visible : Visibility.Collapsed;
        MarkSidebar();
        NextButton.Content = _finished ? "Close" : _nextTexts[page];
        BackButton.IsEnabled = _mode == SetupMode.Install && !_finished && (page == 1 || page == 2);
    }

    /// <summary>Highlights the current step. In the other modes the one visible item is always the current one.</summary>
    private void MarkSidebar()
    {
        int current = Math.Min(_page, 3);
        for (int index = 0; index < _stepItems.Length; index++) _stepItems[index].Background = IsCurrent(index, current) ? ActiveBrush : Brushes.Transparent;
    }

    private bool IsCurrent(int index, int current) => _mode != SetupMode.Install ? index == 1 : index == current;

    private async Task RunPrepare()
    {
        StepOutcome outcome = await Task.Run(() => _install.Prepare(_packageDir));
        ScanStatus.Text = outcome.Ok ? "Scan finished." : outcome.Message;
        ScanDetail.Text = _install.Scan?.Summary ?? "";
        _finished = !outcome.Ok;
        Show(0);
        NextButton.IsEnabled = true;
    }

    private async void NextButton_Click(object sender, RoutedEventArgs e)
    {
        if (_finished) { Close(); return; }
        await _nextActions[_page]();
    }

    private void BackButton_Click(object sender, RoutedEventArgs e) => Show(_page - 1);

    private Task NextScan() { Show(1); return Task.CompletedTask; }

    private Task NextClose() { Close(); return Task.CompletedTask; }

    private Task NextServer()
    {
        if (!AddressIsValid()) return Task.CompletedTask;
        InstallText.Text = DescribeInstall();
        Show(2);
        return Task.CompletedTask;
    }

    private string DescribeInstall()
    {
        string upgrade = _services.Service.Exists() ? "An existing installation will be upgraded and its service restarted. " : "";
        return $"{upgrade}Setup will install EcoWattSense into {_layout.ProgramDir} and start the service. Press Install.";
    }

    private bool AddressIsValid()
    {
        string url = UrlBox.Text.Trim();
        bool good = url.Length == 0 || url.StartsWith("https://", StringComparison.OrdinalIgnoreCase) || url.StartsWith("http://", StringComparison.OrdinalIgnoreCase);
        ServerMessage.Foreground = ErrorBrush;
        ServerMessage.Text = good ? "" : "The server address must start with https://";
        return good;
    }

    private async Task NextInstall()
    {
        SetBusy(true);
        InstallText.Text = "Installing. This can take a minute.";
        InstallRequest request = new(_packageDir, UrlBox.Text.Trim(), KeyBox.Password, SendBox.IsChecked == true);
        StepOutcome outcome = await Task.Run(() => _install.Install(request));
        KeyBox.Clear();
        SetBusy(false);
        FinishInstall(outcome);
    }

    private void FinishInstall(StepOutcome outcome)
    {
        _finished = !outcome.Ok;
        InstallText.Text = outcome.Ok ? "Installed." : outcome.Message;
        if (!outcome.Ok) { Show(2); return; }
        ShowCalibration();
    }

    private void ShowCalibration()
    {
        bool offered = _install.Scan?.CalibrationOffered == true;
        CalibrationText.Text = offered
            ? "This computer needs a calibration so that energy can be estimated when it is on mains power. You can run the calibrator now or skip it and run it later."
            : "No calibration is needed on this computer.";
        CalibrationButtons.Visibility = offered ? Visibility.Visible : Visibility.Collapsed;
        Show(3);
        NextButton.IsEnabled = !offered;
    }

    private async void CalibrateButton_Click(object sender, RoutedEventArgs e)
    {
        SetBusy(true);
        CalibrationButtons.IsEnabled = false;
        CalibrationText.Text = "The calibrator is running. Close its window when it has finished.";
        int code = await Task.Run(_install.Calibrate);
        CalibrationText.Text = $"The calibrator has closed (exit code {code}).";
        SetBusy(false);
    }

    private void SkipButton_Click(object sender, RoutedEventArgs e)
    {
        _install.SkipCalibration();
        CalibrationButtons.IsEnabled = false;
        CalibrationText.Text = "Calibration skipped. You can run EnergyCalibrator.exe later from the program folder.";
        NextButton.IsEnabled = true;
    }

    private async Task NextSaveSettings()
    {
        if (!AddressIsValid()) return;
        SetBusy(true);
        string device = DeviceBox.SelectedItem as string ?? "desktop";
        string url = UrlBox.Text.Trim();
        string key = KeyBox.Password;
        bool send = SendBox.IsChecked == true;
        bool autostart = AutostartBox.IsChecked == true;
        StepOutcome outcome = await Task.Run(() => _maintenance.SaveSettings(url, key, send, device, autostart));
        KeyBox.Clear();
        SetBusy(false);
        ServerMessage.Foreground = outcome.Ok ? GoodBrush : ErrorBrush;
        ServerMessage.Text = outcome.Ok ? "Saved. The service was restarted." : outcome.Message;
        _finished = outcome.Ok;
        Show(1);
    }

    private async Task NextUninstall()
    {
        SetBusy(true);
        bool removeData = RemoveDataBox.IsChecked == true;
        UninstallText.Text = "Removing EcoWattSense.";
        StepOutcome outcome = await Task.Run(() => _maintenance.Uninstall(removeData));
        SetBusy(false);
        UninstallText.Text = outcome.Ok ? "EcoWattSense was removed." : outcome.Message;
        _finished = true;
        Show(4);
    }

    /// <summary>Removes the temporary copy Setup ran from during an uninstall, after the window is closed.</summary>
    protected override void OnClosed(EventArgs e)
    {
        base.OnClosed(e);
        if (_tempCopyDir == null) return;
        _services.Processes.StartDetached("cmd.exe", $"/c ping 127.0.0.1 -n 4 >nul & rmdir /s /q \"{_tempCopyDir}\"");
    }

    private void SetBusy(bool busy)
    {
        NextButton.IsEnabled = !busy;
        BackButton.IsEnabled = !busy && _mode == SetupMode.Install && !_finished && (_page == 1 || _page == 2);
    }

    /// <summary>Fills the Server page from service.ini and tells whether a key is stored.</summary>
    private void LoadServerValues()
    {
        string? ini = _services.Files.ReadText(_layout.IniFile);
        UrlBox.Text = IniEditor.Get(ini, "server_url");
        _hadUrl = UrlBox.Text.Length > 0;
        SendBox.IsChecked = _hadUrl && IniEditor.Get(ini, "server_enabled") != "0";
        _keyStored = _services.Files.FileExists(_layout.KeyFile);
        KeyHint.Text = _keyStored ? "Server key is set" : "Server key";
        DeviceBox.ItemsSource = new[] { "laptop", "desktop" };
        AutostartBox.IsChecked = _services.Tasks.Exists(SetupLayout.TaskName);
        string device = IniEditor.Get(ini, "device_type");
        DeviceBox.SelectedItem = device == "laptop" ? "laptop" : "desktop";
    }

    private void KeyBox_PasswordChanged(object sender, RoutedEventArgs e) =>
        KeyHint.Visibility = KeyBox.Password.Length == 0 ? Visibility.Visible : Visibility.Collapsed;

    /// <summary>Ticks "Send reports" when an address appears and clears it when the address is emptied.</summary>
    private void UrlBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        bool hasUrl = UrlBox.Text.Trim().Length > 0;
        bool changed = hasUrl != _hadUrl;
        _hadUrl = hasUrl;
        if (changed) SendBox.IsChecked = hasUrl;
    }
}
