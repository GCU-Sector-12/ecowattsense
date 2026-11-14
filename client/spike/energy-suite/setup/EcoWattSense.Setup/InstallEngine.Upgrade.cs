// InstallEngine.Upgrade.cs: the upgrade mode of Setup (`Setup.exe /upgrade`, SPEC section 10).
// Who uses it: the window calls CanUpgrade and ChooseMode before it opens, then Upgrade; the tests call the same with fakes.
// An upgrade asks nothing: it keeps the configuration, the key, the device type and the computer name, so it never runs
// --configure and never touches the name. It uses the same steps, the same log and the same undo as the install.
namespace EcoWattSense.Setup;

public sealed partial class InstallEngine
{
    /// <summary>True when an installation exists: the service is registered and the program folder is there.</summary>
    /// <param name="services">The operations.</param>
    /// <param name="layout">The folders.</param>
    public static bool CanUpgrade(SetupServices services, SetupLayout layout) =>
        services.Service.Exists() && services.Files.DirectoryExists(layout.ProgramDir);

    /// <summary>Chooses the mode to run: an upgrade falls back to the normal install pages when there is nothing to upgrade.</summary>
    /// <param name="requested">The mode from the command line.</param>
    /// <param name="services">The operations.</param>
    /// <param name="layout">The folders.</param>
    /// <returns>The mode to open the window in.</returns>
    public static SetupMode ChooseMode(SetupMode requested, SetupServices services, SetupLayout layout)
    {
        if (requested != SetupMode.Upgrade) return requested;
        return CanUpgrade(services, layout) ? SetupMode.Upgrade : SetupMode.Install;
    }

    /// <summary>
    /// Runs the upgrade: close the client, stop the service, copy the programs, keep the data folder rights, start the
    /// service, write the autostart task. Undoes the finished steps when one fails. After a good run it starts the client
    /// as the logged-on user; a client that does not start is a note, never a failure.
    /// </summary>
    /// <param name="packageDir">The folder Setup runs from; the whole folder is copied.</param>
    /// <param name="version">The version text for the message "Updated to &lt;version&gt;.".</param>
    /// <param name="progress">Gets a report for each step change and each file copied; null for none.</param>
    /// <returns>The outcome; on success the message is "Updated to &lt;version&gt;.".</returns>
    public StepOutcome Upgrade(string packageDir, string version, IProgress<InstallProgress>? progress = null)
    {
        _request = new InstallRequest(packageDir, "", "", false);
        _progress = progress;
        InstallStep[] steps =
        [
            new("close_client", () => ClientCloser.Run(_services, _log), null),
            new("stop_old_service", StepStopOld, UndoStopOld),
            new("copy_programs", StepCopy, UndoCopy),
            new("data_folders", StepFolders, null),
            new("start_service", StepStart, UndoStart),
            new("autostart_task", StepTask, UndoTask),
        ];
        StepOutcome outcome = _runner.Run(steps, true, "upgrade", progress);
        return outcome.Ok ? AfterUpgrade(version) : outcome;
    }

    /// <summary>Starts the client and builds the final message.</summary>
    private StepOutcome AfterUpgrade(string version)
    {
        int code = Finish(true);
        string note = code == 0 ? "" : "EcoWattSense could not be started. Start EcoWattSense.Client.Gui.exe from the program folder.";
        return new StepOutcome(true, $"Updated to {version}.", note);
    }

    /// <summary>The device type for the autostart task: the scan result of an install, else the stored value, else desktop.</summary>
    private string ClientDeviceType()
    {
        string stored = IniEditor.Get(_services.Files.ReadText(_layout.IniFile), "device_type");
        string type = Scan?.DeviceType ?? stored;
        return type == "laptop" ? "laptop" : "desktop";
    }
}
