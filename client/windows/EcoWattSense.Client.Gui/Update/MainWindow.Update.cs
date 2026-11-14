// MainWindow.Update.cs: the "Updates" section of the Settings page (viewer mode, SPEC 9, level 1 update).
// Who uses it: StartSettings (check at client start) and ShowPage (check each time the Settings page opens) in
// MainWindow.Settings.cs, and the click handler of the Update button in MainWindow.xaml. There is no timer.
// What it does: asks UpdateChecker for latest.json, shows the state, shows one tray balloon for each newer version
// (remembered in %LOCALAPPDATA%\EcoWattSense\update-seen.txt), and on click downloads the ZIP, unpacks it and starts
// Setup.exe /upgrade elevated. When Setup started, the client closes itself. The pure rules are in UpdateRules.cs.
#nullable enable
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using EcoWattSense.Client.Gui.Update;
using Forms = System.Windows.Forms;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow
{
    private const string UpdateSeenFileName = "update-seen.txt";
    private const string UpdateSetupArguments = "/upgrade";

    private UpdateManifest? updateManifest;
    private bool updateBusy;

    /// <summary>The version of this client without the "+build" suffix.</summary>
    private static string OwnVersion => UpdateRules.StripBuild(
        typeof(MainWindow).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion);

    /// <summary>The file that remembers the newest version the balloon was shown for.</summary>
    private static string UpdateSeenPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EcoWattSense", UpdateSeenFileName);

    /// <summary>Checks latest.json and shows the result. Does nothing while a check or a download runs.</summary>
    /// <returns>A task that ends when the section shows the result.</returns>
    private async Task CheckForUpdateAsync()
    {
        SettingsClientVersion.Text = OwnVersion;
        if (updateBusy) return;
        updateBusy = true;
        ShowUpdate(UpdateState.Checking, null);
        UpdateManifest? manifest = await UpdateChecker.FetchAsync();
        UpdateState state = UpdateRules.StateFor(OwnVersion, manifest);
        updateManifest = state == UpdateState.Available ? manifest : null;
        ShowUpdate(state, updateManifest?.Version);
        updateBusy = false;
        if (updateManifest is not null) NotifyOnce(updateManifest.Version);
    }

    /// <summary>Fills the section.</summary>
    /// <param name="state">The state of the check.</param>
    /// <param name="available">The newer version, or null.</param>
    private void ShowUpdate(UpdateState state, string? available)
    {
        UpdateView view = UpdateRules.View(OwnVersion, state, available);
        UpdateVersionText.Text = view.VersionLine;
        UpdateStatusText.Text = view.StatusLine;
        UpdateButton.Visibility = view.ShowButton ? Visibility.Visible : Visibility.Collapsed;
        UpdateButton.IsEnabled = true;
        UpdateNote.Text = "";
    }

    /// <summary>Shows the tray balloon the first time a version is found, and remembers it.</summary>
    /// <param name="version">The newer version.</param>
    private void NotifyOnce(string version)
    {
        if (!UpdateRules.ShouldNotify(version, ReadSeen())) return;
        trayIcon.ShowBalloonTip(8000, "EcoWattSense", $"EcoWattSense {version} is available. Open Settings to update.", Forms.ToolTipIcon.Info);
        WriteSeen(version);
    }

    /// <summary>Reads update-seen.txt.</summary>
    /// <returns>The file text, or null when the file is missing or cannot be read.</returns>
    private static string? ReadSeen()
    {
        try { return File.ReadAllText(UpdateSeenPath); }
        catch (IOException) { return null; }
        catch (UnauthorizedAccessException) { return null; }
    }

    /// <summary>Writes update-seen.txt. A failure only means the balloon may show again.</summary>
    /// <param name="version">The version to remember.</param>
    private static void WriteSeen(string version)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(UpdateSeenPath)!);
            File.WriteAllText(UpdateSeenPath, version);
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }

    /// <summary>Shows the download progress in the section.</summary>
    /// <param name="received">The bytes received so far.</param>
    private void OnUpdateProgress(long received)
        => UpdateNote.Text = UpdateRules.ProgressLine(received, updateManifest?.Size ?? 0);

    /// <summary>Downloads, unpacks and starts Setup.exe /upgrade; closes the client when Setup started.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private async void OnUpdateClick(object sender, RoutedEventArgs e)
    {
        UpdateManifest? manifest = updateManifest;
        if (manifest is null || updateBusy) return;
        updateBusy = true;
        UpdateButton.IsEnabled = false;
        string note = await RunUpdateAsync(manifest);
        updateBusy = false;
        UpdateButton.IsEnabled = true;
        UpdateNote.Text = note;
        if (note.Length == 0) await CloseForUninstallAsync();
    }

    /// <summary>Does the three steps of the update.</summary>
    /// <param name="manifest">The checked manifest.</param>
    /// <returns>An empty text when Setup started; otherwise the line for the user.</returns>
    private async Task<string> RunUpdateAsync(UpdateManifest manifest)
    {
        DownloadResult download = await UpdateInstaller.DownloadAsync(manifest, new Progress<long>(OnUpdateProgress));
        if (!download.Ok) return download.Note;
        UpdateNote.Text = "Unpacking...";
        string? setup = await UpdateInstaller.ExtractAsync(download.Folder, download.ZipPath);
        if (setup is null) return CleanUp(download.Folder, "The update could not be unpacked.");
        (bool started, string note) = TryStartSetup(setup, UpdateSetupArguments);
        if (started) return "";
        return CleanUp(download.Folder, note.Length > 0 ? note : "Update was not started.");
    }

    /// <summary>Deletes the update folder after a step failed.</summary>
    /// <param name="folder">The temporary folder.</param>
    /// <param name="note">The line for the user.</param>
    /// <returns>The note, unchanged.</returns>
    private static string CleanUp(string folder, string note)
    {
        UpdateInstaller.DeleteFolder(folder);
        return note;
    }
}
