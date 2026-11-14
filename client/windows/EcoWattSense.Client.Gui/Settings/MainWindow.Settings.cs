// MainWindow.Settings.cs: the Settings page of the viewer mode (SPEC 9).
// Who uses it: StartViewer (start), RefreshViewer (new values) in MainWindow.Viewer.cs, and the XAML click handlers.
// The sidebar has two items under the logo: "This computer" and "Settings". The Settings page is read-only: it shows
// the values from status.json. "Change..." starts Setup.exe /settings elevated (UAC), "Start the client with Windows"
// runs the existing Install-Autostart.ps1 or Uninstall-Autostart.ps1 (per-user Task Scheduler task, no administrator rights).
#nullable enable
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media;
using EcoWattSense.Client.Gui.Settings;
using EcoWattSense.Client.Gui.Viewer;
using Brush = System.Windows.Media.Brush;
using Brushes = System.Windows.Media.Brushes;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow
{
    private const int ErrorCancelledByUser = 1223;
    private static readonly Brush SelectedNavBrush = FrozenBrush(0x28, 0x5E, 0x50);

    private SettingsInfo settingsInfo = SettingsInfo.Unknown;
    private bool settingsPageShown;

    /// <summary>Shows the Settings item in the sidebar and fills the values that never change.</summary>
    private void StartSettings()
    {
        SettingsNav.Visibility = Visibility.Visible;
        SettingsDataFolder.Text = ViewerSources.DataFolder;
        ShowSettings(SettingsReader.Read(ViewerSources.StatusFile));
    }

    /// <summary>Shows the page of the "This computer" item.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnComputerNavClick(object sender, RoutedEventArgs e) => ShowPage(false);

    /// <summary>Shows the Settings page.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnSettingsNavClick(object sender, RoutedEventArgs e) => ShowPage(true);

    /// <summary>Switches the right side between the two pages. In classic mode the Settings item is hidden and nothing changes.</summary>
    /// <param name="settings">True for the Settings page, false for the information page.</param>
    private void ShowPage(bool settings)
    {
        if (SettingsNav.Visibility != Visibility.Visible) return;
        settingsPageShown = settings;
        ViewerPanel.Visibility = settings ? Visibility.Collapsed : Visibility.Visible;
        SettingsPanel.Visibility = settings ? Visibility.Visible : Visibility.Collapsed;
        PageTitle.Text = settings ? "Settings" : "This computer";
        PageSubtitle.Text = settings ? "How the service and this computer are set up." : "Your computer's activity and monitoring status.";
        ComputerNav.Background = settings ? Brushes.Transparent : SelectedNavBrush;
        SettingsNav.Background = settings ? SelectedNavBrush : Brushes.Transparent;
        if (settings) _ = RefreshAutostartAsync();
    }

    /// <summary>Fills the page from the checked status.json fields.</summary>
    /// <param name="info">The fields; missing or wrong ones are null and show as "unknown".</param>
    private void ShowSettings(SettingsInfo info)
    {
        settingsInfo = info;
        SettingsAddress.Text = SettingsText.Address(info.ServerUrl);
        SettingsKey.Text = SettingsText.Key(info.KeySet);
        SettingsSending.Text = SettingsText.Sending(info.ServerEnabled);
        SettingsDeviceType.Text = SettingsText.Device(info.DeviceType);
        SettingsInterval.Text = SettingsText.Minutes(info.IntervalSeconds);
        SettingsIdle.Text = SettingsText.Minutes(info.IdleSeconds);
        SettingsUnused.Text = SettingsText.Minutes(info.UnusedSeconds);
        SettingsVersion.Text = SettingsText.Version(info.ServiceVersion);
        ShowSetupButton();
    }

    /// <summary>Enables the Change... button when Setup.exe is found, and says so when it is not.</summary>
    private void ShowSetupButton()
    {
        bool found = ProgramLocator.FindSetup() is not null;
        ChangeServerButton.IsEnabled = found;
        ChangeServerNote.Text = found ? "" : "Setup.exe not found.";
    }

    /// <summary>Starts Setup.exe with /settings, elevated.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnChangeServerClick(object sender, RoutedEventArgs e)
    {
        string? setup = ProgramLocator.FindSetup();
        ChangeServerNote.Text = setup is null ? "Setup.exe not found." : StartSetup(setup);
    }

    /// <summary>Runs Setup.exe /settings with the "runas" verb, so Windows asks for administrator rights.</summary>
    /// <param name="setup">The full path of Setup.exe.</param>
    /// <returns>An empty text when Setup started or the user cancelled the prompt (not an error); otherwise a short error line.</returns>
    private static string StartSetup(string setup)
    {
        var info = new ProcessStartInfo(setup, "/settings")
        {
            UseShellExecute = true,
            Verb = "runas",
            WorkingDirectory = Path.GetDirectoryName(setup) ?? ""
        };
        try { Process.Start(info)?.Dispose(); return ""; }
        catch (Win32Exception exception) when (exception.NativeErrorCode == ErrorCancelledByUser) { return ""; }
        catch (Win32Exception) { return "Setup could not be started."; }
    }

    /// <summary>Registers or removes the autostart task when the user clicks the check box.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private async void OnAutostartClick(object sender, RoutedEventArgs e)
    {
        bool wanted = AutostartCheck.IsChecked == true;
        string? deviceType = settingsInfo.DeviceType;
        AutostartCheck.IsEnabled = false;
        bool done = await Task.Run(() => ApplyAutostart(wanted, deviceType));
        await RefreshAutostartAsync();
        AutostartNote.Text = done ? AutostartNote.Text : "Could not change this setting.";
    }

    /// <summary>Runs the script for the wanted state.</summary>
    /// <param name="enable">True to register the task, false to remove it.</param>
    /// <param name="deviceType">"laptop" or "desktop" for the register script, or null when unknown.</param>
    /// <returns>True when the script ended with exit code 0.</returns>
    private static bool ApplyAutostart(bool enable, string? deviceType)
        => enable ? EnableAutostart(deviceType) : DisableAutostart();

    /// <summary>Runs Install-Autostart.ps1 for this client exe.</summary>
    /// <param name="deviceType">"laptop" or "desktop", or null when unknown.</param>
    /// <returns>True when the script ended with exit code 0; false when a script, the exe path or the device type is missing.</returns>
    private static bool EnableAutostart(string? deviceType)
    {
        string? script = ProgramLocator.FindHere(ProgramLocator.InstallAutostartName);
        string? exe = Environment.ProcessPath;
        bool ready = script is not null && exe is not null && deviceType is not null;
        return ready && AutostartTask.Enable(script!, deviceType!, exe!);
    }

    /// <summary>Runs Uninstall-Autostart.ps1.</summary>
    /// <returns>True when the script ended with exit code 0; false when the script is missing.</returns>
    private static bool DisableAutostart()
    {
        string? script = ProgramLocator.FindHere(ProgramLocator.UninstallAutostartName);
        return script is not null && AutostartTask.Disable(script);
    }

    /// <summary>Reads the task state off the window thread and sets the check box and its line.</summary>
    /// <returns>A task that ends when the check box is set.</returns>
    private async Task RefreshAutostartAsync()
    {
        bool? registered = await Task.Run(AutostartTask.IsRegistered);
        bool scripts = ProgramLocator.FindHere(ProgramLocator.InstallAutostartName) is not null;
        bool deviceKnown = settingsInfo.DeviceType is not null;
        bool canChange = registered is not null && scripts && (registered == true || deviceKnown);
        AutostartCheck.IsChecked = registered == true;
        AutostartCheck.IsEnabled = canChange;
        AutostartNote.Text = AutostartNoteText(registered, scripts, deviceKnown);
    }

    /// <summary>Gives the line that says why the check box cannot be used.</summary>
    /// <param name="registered">True when the task exists, false when not, null when unknown.</param>
    /// <param name="scripts">True when Install-Autostart.ps1 was found.</param>
    /// <param name="deviceKnown">True when status.json gave the device type.</param>
    /// <returns>A short line, or an empty text when the check box can be used.</returns>
    private static string AutostartNoteText(bool? registered, bool scripts, bool deviceKnown)
    {
        if (registered is null) return "The autostart setting could not be read.";
        if (!scripts) return "Install-Autostart.ps1 not found.";
        if (registered == false && !deviceKnown) return "The device type is unknown.";
        return "";
    }
}
