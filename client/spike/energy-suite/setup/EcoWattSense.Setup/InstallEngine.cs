// InstallEngine.cs: the steps of Setup (SPEC section 10), the step runner with undo, and the configuration writer.
// Who uses it: the window (SetupWindow) calls Prepare, Install and the calibration methods; the tests call the same
// methods with fake operations. Nothing here knows WPF. Each step writes one line to install.log; a failed step undoes
// the finished steps in reverse order. The server key travels only as standard input text of ews-service.exe --configure:
// it is never logged, never put on a command line and never written to a file by Setup.
namespace EcoWattSense.Setup;

/// <summary>What the person entered on the Server page, plus where the package is.</summary>
/// <param name="PackageDir">The folder Setup runs from; the whole folder is copied.</param>
/// <param name="Url">Server address; empty keeps the stored one.</param>
/// <param name="Key">Server key; empty keeps the stored one.</param>
/// <param name="SendReports">True to set server_enabled=1.</param>
/// <param name="DeviceName">The computer name typed on the Server page; null leaves the name alone, empty removes it.</param>
public sealed record InstallRequest(string PackageDir, string Url, string Key, bool SendReports, string? DeviceName = null);

/// <summary>The end of a run of steps.</summary>
/// <param name="Ok">True when every step worked.</param>
/// <param name="Message">One clear message for the person. Holds no key and no URL.</param>
/// <param name="Note">An extra line shown after a good run, for example a computer name that could not be sent; empty when none.</param>
public sealed record StepOutcome(bool Ok, string Message, string Note = "");

/// <summary>One step: a name for the log, the work and the undo (null when there is nothing to undo).</summary>
/// <param name="Name">Fixed step name for the log.</param>
/// <param name="Run">The work; returns 0 or an error code.</param>
/// <param name="Undo">Reverses the work, or null.</param>
public sealed record InstallStep(string Name, Func<int> Run, Action? Undo);

/// <summary>Runs steps in order and undoes the finished ones in reverse order when one fails.</summary>
public sealed class StepRunner
{
    private readonly SetupLog _log;

    /// <summary>Creates a runner that logs to the given log.</summary>
    public StepRunner(SetupLog log) => _log = log;

    /// <summary>Runs the steps. Stops at the first failure.</summary>
    /// <param name="steps">The steps in order.</param>
    /// <param name="undoOnFailure">True to undo the finished steps when one fails.</param>
    /// <param name="what">Word for the message: install, uninstall or settings.</param>
    /// <param name="progress">Gets a report when a step starts, ends well or fails; null for none.</param>
    /// <returns>The outcome with a message.</returns>
    public StepOutcome Run(IReadOnlyList<InstallStep> steps, bool undoOnFailure, string what, IProgress<InstallProgress>? progress = null)
    {
        List<InstallStep> done = [];
        InstallStep? failed = null;
        int code = 0;
        for (int index = 0; index < steps.Count && failed == null; index++)
        {
            code = RunOne(steps[index], done, progress);
            failed = code == 0 ? null : steps[index];
        }
        if (failed == null) return new StepOutcome(true, "Done.");
        if (undoOnFailure) UndoAll(done);
        string tail = DoneText(done, undoOnFailure);
        string hint = Hints.GetValueOrDefault(failed.Name, "");
        StepOutcome outcome = new(false, $"The {what} stopped at the step \"{failed.Name}\" (error {code}). {hint} {tail} See install.log.");
        progress?.Report(new InstallProgress(failed.Name, StepState.Failed, outcome.Message));
        return outcome;
    }

    /// <summary>Names what was done before the failure in plain words, so the message never claims that nothing was done.</summary>
    private static string DoneText(List<InstallStep> done, bool undone)
    {
        string words = done.Count == 0 ? "no step" : string.Join(", ", done.Select(step => InstallTitles.For(step.Name)));
        string undoneText = undone && done.Count > 0 ? " These steps were undone." : "";
        return $"Done before it: {words}.{undoneText}";
    }

    /// <summary>A short help text for the steps that fail most often.</summary>
    private static readonly Dictionary<string, string> Hints = new()
    {
        ["check"] = "Setup needs Windows 11 (64-bit) and administrator rights.",
        ["scan"] = "The scanner did not finish.",
        ["stop_old_service"] = "The old service did not stop.",
        ["close_client"] = "A running EcoWattSense program could not be closed; close it and try again.",
        ["copy_programs"] = "A file in the program folder may be in use; close EcoWattSense and try again.",
        ["configure"] = "The server address or key was not accepted.",
    };

    private int RunOne(InstallStep step, List<InstallStep> done, IProgress<InstallProgress>? progress)
    {
        string title = InstallTitles.For(step.Name);
        progress?.Report(new InstallProgress(step.Name, StepState.Working, title));
        int code = step.Run();
        _log.Line(step.Name, code == 0 ? "ok" : "failed", code);
        if (code == 0) done.Add(step);
        if (code == 0) progress?.Report(new InstallProgress(step.Name, StepState.Done, title));
        return code;
    }

    private void UndoAll(List<InstallStep> done)
    {
        for (int index = done.Count - 1; index >= 0; index--)
        {
            UndoOne(done[index]);
        }
    }

    private void UndoOne(InstallStep step)
    {
        step.Undo?.Invoke();
        _log.Line(step.Name, step.Undo == null ? "kept" : "undone", 0);
    }
}

/// <summary>
/// Writes the server settings: address and key through ews-service.exe --configure, then device_type and
/// server_enabled into service.ini (the configure input cannot carry them).
/// </summary>
public sealed class ConfigWriter
{
    private readonly SetupServices _services;
    private readonly SetupLayout _layout;

    /// <summary>Creates the writer.</summary>
    public ConfigWriter(SetupServices services, SetupLayout layout)
    {
        _services = services;
        _layout = layout;
    }

    /// <summary>Writes the settings. Empty address or key keeps the stored value, so no line is sent for it.</summary>
    /// <param name="url">Server address or empty.</param>
    /// <param name="key">Server key or empty.</param>
    /// <param name="sendReports">The value for server_enabled.</param>
    /// <param name="deviceType">The value for device_type.</param>
    /// <returns>0 or an error code (87 for a value with a line break).</returns>
    public int Write(string url, string key, bool sendReports, string deviceType)
    {
        if (HasLineBreak(url) || HasLineBreak(key) || HasLineBreak(deviceType)) return 87;
        int code = RunConfigure(url, key);
        if (code != 0) return code;
        string text = _services.Files.ReadText(_layout.IniFile) ?? "";
        text = IniEditor.Set(text, "device_type", deviceType);
        text = IniEditor.Set(text, "server_enabled", sendReports ? "1" : "0");
        return _services.Files.WriteText(_layout.IniFile, text);
    }

    private static bool HasLineBreak(string text) => text.Contains('\n') || text.Contains('\r');

    private int RunConfigure(string url, string key)
    {
        string input = (url.Length > 0 ? "url=" + url + "\n" : "") + (key.Length > 0 ? "key=" + key + "\n" : "");
        if (input.Length == 0) return 0;
        return _services.Processes.Run(_layout.ServiceExe, $"--configure --data-dir \"{_layout.DataDir}\"", input);
    }
}

/// <summary>The install: check, scan, copy, rights, configuration, service, autostart, calibration.</summary>
public sealed partial class InstallEngine
{
    private readonly SetupServices _services;
    private readonly SetupLayout _layout;
    private readonly SetupLog _log;
    private readonly StepRunner _runner;
    private readonly ConfigWriter _config;
    private readonly DeviceNameSync? _deviceName;

    private string _packageDir = "";
    private InstallRequest _request = new("", "", "", false);
    private bool _serviceExisted;
    private bool _serviceWasRunning;
    private bool _programDirExisted;
    private IProgress<InstallProgress>? _progress;

    /// <summary>Creates the engine.</summary>
    /// <param name="services">The operations.</param>
    /// <param name="layout">The folders.</param>
    /// <param name="log">The install log.</param>
    /// <param name="deviceName">Saves and sends the computer name; null skips the name.</param>
    public InstallEngine(SetupServices services, SetupLayout layout, SetupLog log, DeviceNameSync? deviceName = null)
    {
        _deviceName = deviceName;
        _services = services;
        _layout = layout;
        _log = log;
        _runner = new StepRunner(log);
        _config = new ConfigWriter(services, layout);
    }

    /// <summary>The scan result, set after a successful Prepare.</summary>
    public ScanResult? Scan { get; private set; }

    /// <summary>Steps 1 and 2: check the computer and run the scanner. Changes only the scan folder and the log.</summary>
    /// <param name="packageDir">The folder with ews-scan.exe.</param>
    public StepOutcome Prepare(string packageDir)
    {
        _packageDir = packageDir;
        InstallStep[] steps = [new("check", StepCheck, null), new("scan", StepScan, null)];
        return _runner.Run(steps, false, "check");
    }

    /// <summary>Steps 4 to 8: stop an old service, copy, rights, configuration, service, autostart. Undoes on failure.</summary>
    /// <param name="request">The Server page values and the package folder.</param>
    /// <param name="progress">Gets a report for each step change and for each file copied; null for none.</param>
    public StepOutcome Install(InstallRequest request, IProgress<InstallProgress>? progress = null)
    {
        _request = request;
        _progress = progress;
        InstallStep[] steps =
        [
            new("stop_old_service", StepStopOld, UndoStopOld),
            new("close_client", () => ClientCloser.Run(_services, _log), null),
            new("copy_programs", StepCopy, UndoCopy),
            new("data_folders", StepFolders, null),
            new("configure", StepConfigure, null),
            new("register_service", StepRegister, UndoRegister),
            new("start_service", StepStart, UndoStart),
            new("autostart_task", StepTask, UndoTask),
        ];
        StepOutcome outcome = _runner.Run(steps, true, "install", progress);
        return outcome.Ok ? WithDeviceName(outcome) : outcome;
    }

    /// <summary>After a good install: saves and sends the computer name. A problem is only a note, never a failure.</summary>
    private StepOutcome WithDeviceName(StepOutcome outcome)
    {
        if (_deviceName == null || _request.DeviceName == null) return outcome;
        const string name = "device_name";
        _progress?.Report(new InstallProgress(name, StepState.Working, InstallTitles.For(name)));
        string deviceType = Scan?.DeviceType ?? "desktop";
        StepOutcome sent = _deviceName.Apply(_request.DeviceName, _request.Url, _request.Key, deviceType);
        StepState state = sent.Message.Length == 0 ? StepState.Done : StepState.Failed;
        string text = sent.Message.Length == 0 ? InstallTitles.For(name) : sent.Message;
        _progress?.Report(new InstallProgress(name, state, text));
        return outcome with { Note = sent.Message };
    }

    /// <summary>
    /// The Finish button: starts the client as the logged-on user, not elevated, without waiting, when the box was ticked.
    /// A failure is logged as `start_client failed` and returned; it never fails the install.
    /// </summary>
    /// <param name="startClient">True when "Start EcoWattSense now" was ticked.</param>
    /// <returns>0 when the client started or was not asked for, otherwise the error code or -1.</returns>
    public int Finish(bool startClient)
    {
        if (!startClient) return 0;
        int code = _services.Processes.StartAsLoggedOnUser(_layout.ClientExe);
        _log.Line("start_client", code == 0 ? "started" : "failed", code);
        return code;
    }

    /// <summary>Reports each file copied as "Copying programs n of total" to the install progress.</summary>
    private sealed class CopyReporter(IProgress<InstallProgress>? target, int total) : IProgress<int>
    {
        /// <summary>Sends one report for the number of files copied so far.</summary>
        public void Report(int copied)
        {
            string text = $"{InstallTitles.For("copy_programs")} {copied} of {total}";
            target?.Report(new InstallProgress("copy_programs", StepState.Working, text, copied, total));
        }
    }

    /// <summary>Step 9, "Calibrate now": runs the calibrator as the logged-on user and logs its exit code.</summary>
    /// <returns>The exit code, or -1 when it could not start.</returns>
    public int Calibrate()
    {
        int code = _services.Processes.RunAsLoggedOnUser(_layout.CalibratorExe);
        _log.Line("calibration", "run_exit", code);
        return code;
    }

    /// <summary>Step 9, "Skip": logs the choice.</summary>
    public void SkipCalibration() => _log.Line("calibration", "skipped", 0);

    private int StepCheck()
    {
        if (!_services.System.IsWindows11X64()) return 1;
        return _services.System.IsAdministrator() ? 0 : 2;
    }

    private int StepScan()
    {
        string scanDir = _layout.Data("scan");
        int code = _services.Files.CreateDirectory(scanDir);
        if (code != 0) return code;
        string exe = Path.Combine(_packageDir, "ews-scan.exe");
        code = _services.Processes.Run(exe, $"--out \"{scanDir}\" --context admin", null);
        if (code != 0) return code;
        Scan = ScanResult.Parse(_services.Files.ReadText(_layout.CapabilitiesFile));
        if (Scan == null) return 3;
        _log.Line("scan_result", $"{Scan.Level}_{Scan.Calibration}_sources{Scan.SourcesFound}", 0);
        return 0;
    }

    private int StepStopOld()
    {
        _serviceExisted = _services.Service.Exists();
        _serviceWasRunning = _serviceExisted && _services.Service.IsRunning();
        return _serviceWasRunning ? ServiceStop.Run(_services, _log) : 0;
    }

    private void UndoStopOld()
    {
        if (_serviceWasRunning) _services.Service.Start();
    }

    private int StepCopy()
    {
        _programDirExisted = _services.Files.DirectoryExists(_layout.ProgramDir);
        bool sameFolder = string.Equals(Path.GetFullPath(_request.PackageDir).TrimEnd('\\'), Path.GetFullPath(_layout.ProgramDir).TrimEnd('\\'), StringComparison.OrdinalIgnoreCase);
        if (sameFolder) return 0;
        CopyReporter reporter = new(_progress, _services.Files.CountFiles(_request.PackageDir));
        return new ProgramSwap(_services, _layout, _log).Run(_request.PackageDir, reporter);
    }

    private void UndoCopy()
    {
        new ProgramSwap(_services, _layout, _log).Undo(_programDirExisted);
    }

    private int StepFolders()
    {
        IReadOnlyList<FolderPlan> plan = AclPlan.Build(_layout.DataDir, ServiceSid.Compute(SetupLayout.ServiceName));
        int code = 0;
        for (int index = 0; index < plan.Count && code == 0; index++)
        {
            code = ApplyFolder(plan[index]);
        }
        return code;
    }

    private int ApplyFolder(FolderPlan plan)
    {
        bool existed = _services.Files.DirectoryExists(plan.Path);
        int code = _services.Files.CreateDirectory(plan.Path);
        if (code != 0) return code;
        code = existed || plan.EntriesWhenNew.Count == 0 ? 0 : _services.Acls.Apply(plan.Path, AclMode.Add, plan.EntriesWhenNew);
        if (code != 0) return code;
        return _services.Acls.Apply(plan.Path, plan.Mode, plan.Entries);
    }

    private int StepConfigure()
    {
        string deviceType = Scan?.DeviceType ?? "desktop";
        return _config.Write(_request.Url, _request.Key, _request.SendReports, deviceType);
    }

    private int StepRegister()
    {
        ServiceSpec spec = new(SetupLayout.ServiceName, SetupLayout.ServiceDisplayName, _layout.ServiceExe,
            SetupLayout.ServiceAccount, "Measures the energy use of this computer and reports it when a server is set.");
        return _services.Service.Register(spec);
    }

    private void UndoRegister()
    {
        if (!_serviceExisted) _services.Service.Delete();
    }

    private int StepStart() => _services.Service.Start();

    private void UndoStart() => _services.Service.Stop();

    private int StepTask()
    {
        _services.Tasks.Delete(SetupLayout.TaskName);
        string clientType = ClientDeviceType();
        string xml = TaskXml.Build(_layout.ClientExe, _layout.ProgramDir, clientType);
        return _services.Tasks.Register(SetupLayout.TaskName, xml);
    }

    private void UndoTask() => _services.Tasks.Delete(SetupLayout.TaskName);
}
