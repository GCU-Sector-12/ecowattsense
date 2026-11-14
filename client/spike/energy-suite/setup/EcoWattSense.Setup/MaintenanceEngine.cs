// MaintenanceEngine.cs: the two other modes of Setup: `Setup.exe /uninstall` and `Setup.exe /settings` (SPEC section 10).
// Who uses it: the window and the tests. Uninstall removes the service, the task and the programs, and the data only when
// asked. Settings changes only the server address, key, send switch and device type, then restarts the service, because
// the service reads its configuration only at start. Every step writes one line to install.log.
namespace EcoWattSense.Setup;

/// <summary>Uninstall and settings.</summary>
public sealed class MaintenanceEngine
{
    private readonly SetupServices _services;
    private readonly SetupLayout _layout;
    private readonly SetupLog _log;
    private readonly StepRunner _runner;
    private readonly ConfigWriter _config;
    private readonly DeviceNameSync? _deviceName;

    private bool _serviceWasRunning;
    private string _url = "";
    private string _key = "";
    private bool _sendReports;
    private string _deviceType = "desktop";
    private bool _autostart;

    /// <summary>Creates the engine.</summary>
    /// <param name="services">The operations.</param>
    /// <param name="layout">The folders.</param>
    /// <param name="log">The install log.</param>
    /// <param name="deviceName">Saves and sends the computer name; null skips the name.</param>
    public MaintenanceEngine(SetupServices services, SetupLayout layout, SetupLog log, DeviceNameSync? deviceName = null)
    {
        _deviceName = deviceName;
        _services = services;
        _layout = layout;
        _log = log;
        _runner = new StepRunner(log);
        _config = new ConfigWriter(services, layout);
    }

    /// <summary>
    /// Stops and deletes the service, deletes the autostart task and the programs folder, and the seven service data
    /// folders when asked. The Client folder is never removed. A failed step stops the uninstall and says which one.
    /// </summary>
    /// <param name="removeData">True when the person ticked "Remove data".</param>
    public StepOutcome Uninstall(bool removeData)
    {
        List<InstallStep> steps =
        [
            new("stop_service", StepStop, null),
            new("delete_service", StepDeleteService, null),
            new("delete_task", StepDeleteTask, null),
            new("close_client", () => ClientCloser.Run(_services, _log), null),
            new("delete_programs", StepDeletePrograms, null),
        ];
        if (removeData) steps.Add(new InstallStep("remove_data", StepRemoveData, null));
        return _runner.Run(steps, false, "uninstall");
    }

    /// <summary>
    /// Saves the Server page values and restarts the service so it reads them. An empty key keeps the stored key.
    /// When a step fails, a service that was running is started again.
    /// </summary>
    /// <param name="url">Server address or empty.</param>
    /// <param name="key">Server key or empty.</param>
    /// <param name="sendReports">The server_enabled switch.</param>
    /// <param name="deviceType">laptop or desktop.</param>
    /// <param name="autostart">True to register the client autostart task for every user, false to remove it.</param>
    /// <param name="computerName">The computer name; null leaves it alone, empty removes it. A problem with it is only a note.</param>
    public StepOutcome SaveSettings(string url, string key, bool sendReports, string deviceType, bool autostart, string? computerName = null)
    {
        _autostart = autostart;
        _url = url;
        _key = key;
        _sendReports = sendReports;
        _deviceType = deviceType;
        InstallStep[] steps =
        [
            new("settings_stop_service", StepStopForSettings, UndoStopForSettings),
            new("settings_write", StepWrite, null),
            new("settings_autostart", StepAutostart, null),
            new("settings_start_service", StepStartAfterSettings, null),
        ];
        StepOutcome outcome = _runner.Run(steps, true, "settings change");
        return outcome.Ok ? WithDeviceName(outcome, computerName) : outcome;
    }

    /// <summary>After a good save: saves and sends the computer name. A problem is only a note, never a failure.</summary>
    private StepOutcome WithDeviceName(StepOutcome outcome, string? computerName)
    {
        if (_deviceName == null || computerName == null) return outcome;
        StepOutcome sent = _deviceName.Apply(computerName, _url, _key, _deviceType);
        return outcome with { Note = sent.Message };
    }

    private int StepStop()
    {
        bool running = _services.Service.Exists() && _services.Service.IsRunning();
        return running ? _services.Service.Stop() : 0;
    }

    private int StepDeleteService() => _services.Service.Delete();

    private int StepDeleteTask() => _services.Tasks.Delete(SetupLayout.TaskName);

    /// <summary>True when the empty program folder stays locked and is removed at the next restart.</summary>
    public bool ProgramFolderAtReboot { get; private set; }

    private const int SharingViolation = 32;

    /// <summary>
    /// Leaves the program folder first (a process whose current directory is inside a folder keeps it locked), deletes it,
    /// retries a sharing violation 3 times 1 second apart, then marks the folder for deletion at the next restart.
    /// </summary>
    private int StepDeletePrograms()
    {
        _services.Processes.SetWorkingDirectory(Path.GetTempPath());
        int code = _services.Files.DeleteDirectory(_layout.ProgramDir);
        for (int retry = 0; retry < 3 && code == SharingViolation; retry++)
        {
            _services.Processes.Wait(1000);
            code = _services.Files.DeleteDirectory(_layout.ProgramDir);
        }
        if (code != SharingViolation) return code;
        int marked = _services.Files.DeleteAtReboot(_layout.ProgramDir);
        ProgramFolderAtReboot = marked == 0;
        if (marked == 0) _log.Line("delete_programs", "at_reboot", 0);
        return marked == 0 ? 0 : code;
    }

    private int StepRemoveData()
    {
        int code = 0;
        for (int index = 0; index < SetupLayout.ServiceFolders.Length && code == 0; index++)
        {
            code = _services.Files.DeleteDirectory(_layout.Data(SetupLayout.ServiceFolders[index]));
        }
        _log.Suspend();
        return code;
    }

    private int StepStopForSettings()
    {
        _serviceWasRunning = _services.Service.Exists() && _services.Service.IsRunning();
        return _serviceWasRunning ? _services.Service.Stop() : 0;
    }

    private void UndoStopForSettings()
    {
        if (_serviceWasRunning) _services.Service.Start();
    }

    private int StepWrite() => _config.Write(_url, _key, _sendReports, _deviceType);

    /// <summary>Removes the task, then registers it again when the box is ticked (same name as the classic client task).</summary>
    private int StepAutostart()
    {
        int code = _services.Tasks.Delete(SetupLayout.TaskName);
        if (!_autostart) return code;
        string xml = TaskXml.Build(_layout.ClientExe, _layout.ProgramDir, _deviceType);
        return _services.Tasks.Register(SetupLayout.TaskName, xml);
    }

    private int StepStartAfterSettings() => _services.Service.Exists() ? _services.Service.Start() : 0;
}
