// Checks.cs: the groups of checks of SetupTests: pure code, the install, the undo and the other modes.
// Each public Run method calls small check methods; every check goes through Harness.Check.
using System.Xml.Linq;
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>Checks of the pure parts: SID, ini editor, scan parser, task XML, arguments.</summary>
public static class PureChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Check(ServiceSid.Compute("TrustedInstaller") == "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464", "service SID matches the known TrustedInstaller SID");
        Check(ServiceSid.Compute("trustedinstaller") == ServiceSid.Compute("TRUSTEDINSTALLER"), "service SID ignores case");
        IniChecks();
        ScanChecks();
        TaskChecks();
        ArgumentChecks();
    }

    private static void IniChecks()
    {
        string text = IniEditor.Set("interval_seconds=300\r\ndevice_type=tablet\r\n", "device_type", "laptop");
        Check(text == "interval_seconds=300\ndevice_type=laptop\n".Replace("device_type=laptop\n", "") + "device_type=laptop\n", "ini: a key is replaced and other lines stay");
        Check(IniEditor.Get(text, "interval_seconds") == "300", "ini: a value is read");
        Check(IniEditor.Get(null, "x") == "", "ini: a missing file gives an empty value");
        Check(IniEditor.Set(null, "server_enabled", "1") == "server_enabled=1\n", "ini: a key is added to a missing file");
    }

    private static void ScanChecks()
    {
        ScanResult? scan = ScanResult.Parse(FakeWorld.ScanJson);
        Check(scan is { Level: "L2", Calibration: "needed", DeviceType: "laptop", SourcesFound: 2, HasBattery: true }, "scan: the sample file is read");
        Check(scan?.CalibrationOffered == true, "scan: calibration is offered when needed and a battery exists");
        Check(ScanResult.Parse("not json") == null && ScanResult.Parse("{}") == null && ScanResult.Parse(null) == null, "scan: bad input gives null");
    }

    private static void TaskChecks()
    {
        string xml = TaskXml.Build(@"C:\Program Files\EcoWattSense\EcoWattSense.Client.Gui.exe", @"C:\Program Files\EcoWattSense", "laptop");
        XDocument document = XDocument.Parse(xml);
        Check(document.Root != null, "task xml: well-formed");
        Check(xml.Contains("<GroupId>S-1-5-32-545</GroupId>") && xml.Contains("LeastPrivilege"), "task xml: group Users, not elevated");
        Check(xml.Contains("<Delay>PT1M</Delay>") && xml.Contains("<LogonTrigger>") && !xml.Contains("<UserId>"), "task xml: log-on trigger for every user, one minute delay");
        Check(xml.Contains("--start-hidden --Reporting:DeviceType=laptop") && !xml.Contains("key", StringComparison.OrdinalIgnoreCase), "task xml: arguments hold no key");
    }

    private static void ArgumentChecks()
    {
        Check(SetupArguments.Parse([]).Mode == SetupMode.Install, "arguments: none is install");
        Check(SetupArguments.Parse(["/UNINSTALL"]).Mode == SetupMode.Uninstall, "arguments: /uninstall");
        Check(SetupArguments.Parse(["/settings"]).Mode == SetupMode.Settings, "arguments: /settings");
        Check(SetupArguments.Parse(["/uninstall", "/tempcopy"]).IsTempCopy, "arguments: /tempcopy");
    }
}

/// <summary>Checks of a good install, an upgrade and the rights.</summary>
public static class InstallChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        FreshInstallChecks();
        KeyChecks();
        AclChecks();
        UpgradeChecks();
        CalibrationChecks();
        PrepareChecks();
    }

    private static void FreshInstallChecks()
    {
        FakeWorld world = new();
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        Check(outcome.Ok, "install: a fresh install works");
        string[] expected =
        [
            "check ok", "scan_result L2_needed_sources2", "scan ok", "stop_old_service ok", "close_client 0", "close_client ok", "copy_new ok", "swap ok", "copy_programs ok", "data_folders ok",
            "configure ok", "register_service ok", "start_service ok", "autostart_task ok",
        ];
        Check(Kit.LogWords(world).SequenceEqual(expected), "install: the steps run and log in the order of SPEC 10");
        List<string> calls = world.Calls;
        bool ordered = calls.IndexOf("copydir") < calls.IndexOf("acl") && calls.IndexOf("acl") < calls.IndexOf("configure")
            && calls.IndexOf("configure") < calls.IndexOf("register") && calls.IndexOf("register") < calls.IndexOf("start") && calls.IndexOf("start") < calls.IndexOf("task");
        Check(ordered, "install: copy, rights, configure, register, start, task happen in that order");
        Check(world.Registered?.Account == @"NT AUTHORITY\LocalService" && world.Registered.Name == "EcoWattSenseService", "install: service name and account as SPEC 5.1");
        Check(world.Registered?.BinaryPath == @"P:\Prog\ews-service.exe", "install: the service runs the installed program");
        Check(world.Tasks.ContainsKey("EcoWattSense Client") && world.Tasks["EcoWattSense Client"].Contains("DeviceType=laptop"), "install: the autostart task is registered with the scan device type");
        string ini = world.Files[Kit.Layout.IniFile];
        Check(IniEditor.Get(ini, "device_type") == "laptop" && IniEditor.Get(ini, "server_enabled") == "1", "install: device_type from the scan and server_enabled are written");
        Check(world.Calls.Contains("task.delete"), "install: an older task of the classic client is replaced");
    }

    private static void KeyChecks()
    {
        FakeWorld world = new();
        Kit.RunInstall(world, Kit.Url, Kit.Key);
        RunCall configure = world.Runs.Single(call => call.Exe.EndsWith("ews-service.exe"));
        Check(configure.Input != null && configure.Input.Contains("key=" + Kit.Key), "key: reaches ews-service --configure through standard input");
        Check(world.Runs.All(call => !call.Arguments.Contains(Kit.Key) && !call.Exe.Contains(Kit.Key)), "key: is on no command line");
        Check(!Kit.LogText(world).Contains(Kit.Key) && !Kit.LogText(world).Contains("secret.example.org"), "key: neither key nor address is in the log");
        Check(world.Files.Values.All(text => !text.Contains(Kit.Key)), "key: Setup wrote it to no file");
    }

    private static void AclChecks()
    {
        FakeWorld world = new();
        Kit.RunInstall(world, "", "");
        string sid = Kit.ServiceSidText;
        AclCall config = world.Acls.Last(call => call.Path == Kit.Layout.Data("config"));
        string[] sids = config.Entries.Select(entry => entry.Sid).Order().ToArray();
        string[] wanted = new[] { AclPlan.System, AclPlan.Admins, sid }.Order().ToArray();
        Check(config.Mode == AclMode.Reset && sids.SequenceEqual(wanted), "rights: config holds only the service SID, Administrators and SYSTEM");
        Check(config.Entries.Single(entry => entry.Sid == sid).Right == AccessRight.Read, "rights: the service SID reads config");
        Check(world.Acls.All(call => call.Entries.All(entry => entry.Sid != "S-1-5-19")), "rights: LocalService is not used anywhere");
        Check(world.Acls.Where(call => call.Path.EndsWith("queue")).All(call => call.Entries.All(entry => entry.Sid != AclPlan.Users)), "rights: Users have no access to queue");
        AclCall calibration = world.Acls.Last(call => call.Path == Kit.Layout.Data("calibration"));
        Check(calibration.Entries.Any(entry => entry.Sid == AclPlan.Users && entry.Right == AccessRight.Modify), "rights: Users modify calibration");
        Check(world.Acls.Count(call => call.Path.EndsWith("Client") && call.Mode == AclMode.Add) == 2, "rights: a new Client folder gets Users modify and the service SID");
        FakeWorld existing = new();
        existing.Directories.Add(Kit.Layout.Data("Client"));
        Kit.RunInstall(existing, "", "");
        Check(existing.Acls.Count(call => call.Path.EndsWith("Client")) == 1, "rights: an existing Client folder only gets the service SID added");
    }

    private static void UpgradeChecks()
    {
        FakeWorld world = Kit.OldInstall();
        StepOutcome outcome = Kit.RunInstall(world, "", "");
        Check(outcome.Ok, "upgrade: works over an existing service");
        Check(world.Calls.IndexOf("stop") >= 0 && world.Calls.IndexOf("stop") < world.Calls.IndexOf("copydir"), "upgrade: the old service stops before the files are copied");
        Check(world.RegisterCount == 1 && world.ServiceExists && !world.Calls.Contains("service.delete"), "upgrade: the service is updated, not deleted");
        Check(world.ServiceRunning, "upgrade: the service runs again at the end");
        FakeWorld same = new();
        SetupLog log = new(same.Services, Kit.Layout);
        InstallEngine engine = new(same.Services, Kit.Layout, log);
        engine.Prepare(Kit.Package);
        engine.Install(new InstallRequest(Kit.Layout.ProgramDir, "", "", false));
        Check(!same.Calls.Contains("copydir"), "upgrade: nothing is copied when Setup runs from the program folder");
    }

    private static void CalibrationChecks()
    {
        FakeWorld world = new();
        SetupLog log = new(world.Services, Kit.Layout);
        InstallEngine engine = new(world.Services, Kit.Layout, log);
        engine.Prepare(Kit.Package);
        Check(engine.Scan?.CalibrationOffered == true, "calibration: offered after a scan that says needed");
        int code = engine.Calibrate();
        Check(code == 7 && Kit.LogText(world).Contains("calibration run_exit 7"), "calibration: the exit code is logged");
        Check(world.Calls.Contains(@"calibrator P:\Prog\EnergyCalibrator.exe"), "calibration: started from the program folder as the logged-on user");
        CalibrationTextChecks.Run();
        engine.SkipCalibration();
        Check(Kit.LogText(world).Contains("calibration skipped 0"), "calibration: the choice to skip is logged");
    }

    private static void PrepareChecks()
    {
        FakeWorld world = new() { Administrator = false };
        StepOutcome outcome = Kit.RunInstall(world, "", "");
        Check(!outcome.Ok && outcome.Message.Contains("check") && world.Runs.Count == 0, "check: without administrator rights Setup stops before the scan");
        FakeWorld old = new() { Windows11 = false };
        Check(!Kit.RunInstall(old, "", "").Ok, "check: a computer that is not Windows 11 x64 is refused");
    }
}

/// <summary>Checks that a failed step undoes the steps before it.</summary>
public static class UndoChecks
{
    private static readonly string[] Operations = ["stop", "copydir", "acl", "configure", "register", "start", "task"];

    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        foreach (string operation in Operations) FailureOnOldInstall(operation);
        foreach (string operation in Operations.Skip(1)) FailureOnFreshInstall(operation);
        ReverseOrderCheck();
        ScanFailureCheck();
    }

    private static void FailureOnOldInstall(string operation)
    {
        FakeWorld world = Kit.OldInstall();
        world.FailOn = operation;
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, "");
        Check(!outcome.Ok && outcome.Message.Contains(operation == "configure" ? "error 1" : "error 5"), $"undo (upgrade): failure at {operation} gives one clear message");
        Check(world.ServiceRunning && world.ServiceExists, $"undo (upgrade): after a failure at {operation} the old service still exists and runs");
        Check(world.Directories.Contains(Kit.Layout.ProgramDir) && world.Tasks.Count == 0, $"undo (upgrade): after a failure at {operation} the old program folder stays and no task is left");
    }

    private static void FailureOnFreshInstall(string operation)
    {
        FakeWorld world = new() { FailOn = operation };
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, "");
        Check(!outcome.Ok && outcome.Message.Contains(operation == "configure" ? "error 1" : "error 5"), $"undo (fresh): failure at {operation} gives one clear message");
        Check(!world.Directories.Contains(Kit.Layout.ProgramDir), $"undo (fresh): after a failure at {operation} no program folder is left");
        Check(!world.ServiceExists && !world.ServiceRunning && world.Tasks.Count == 0, $"undo (fresh): after a failure at {operation} no service and no task are left");
    }

    private static void ReverseOrderCheck()
    {
        FakeWorld world = new() { FailOn = "task" };
        Kit.RunInstall(world, "", "");
        List<string> words = Kit.LogWords(world);
        List<string> tail = words.Skip(words.IndexOf("autostart_task failed")).ToList();
        string[] expected =
        [
            "autostart_task failed", "start_service undone", "register_service undone", "configure kept", "data_folders kept",
            "copy_programs undone", "close_client kept", "stop_old_service undone",
        ];
        Check(tail.SequenceEqual(expected), "undo: the finished steps are undone in reverse order and logged");
    }

    private static void ScanFailureCheck()
    {
        FakeWorld world = new() { FailOn = "scan" };
        StepOutcome outcome = Kit.RunInstall(world, "", "");
        Check(!outcome.Ok && outcome.Message.Contains("scan") && !world.Directories.Contains(Kit.Layout.ProgramDir), "scan failure: stops before anything is installed");
    }
}

/// <summary>Checks of /uninstall and /settings.</summary>
public static class MaintenanceChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        UninstallKeepsData();
        UninstallRemovesData();
        UninstallFailure();
        UninstallLockedFolder();
        UninstallRelaunchDirectory();
        SettingsEmptyKey();
        SettingsWithKey();
        SettingsFailureRestartsService();
        SettingsAutostart();
        LineBreakCheck();
    }

    private static FakeWorld Installed()
    {
        FakeWorld world = new();
        Kit.RunInstall(world, Kit.Url, Kit.Key);
        world.Calls.Clear();
        return world;
    }

    private static MaintenanceEngine Engine(FakeWorld world) => new(world.Services, Kit.Layout, new SetupLog(world.Services, Kit.Layout));

    private static void UninstallKeepsData()
    {
        FakeWorld world = Installed();
        StepOutcome outcome = Engine(world).Uninstall(false);
        Check(outcome.Ok && !world.ServiceExists && world.Tasks.Count == 0 && !world.Directories.Contains(Kit.Layout.ProgramDir), "uninstall: service, task and programs are removed");
        Check(world.Directories.Contains(Kit.Layout.Data("scan")) && world.Files.ContainsKey(Kit.Layout.KeyFile) && world.Files.ContainsKey(Kit.Layout.LogFile), "uninstall: the data stays without Remove data");
        Check(world.Calls.IndexOf("stop") < world.Calls.IndexOf("service.delete"), "uninstall: the service stops before it is deleted");
    }

    private static void UninstallRemovesData()
    {
        FakeWorld world = Installed();
        world.Directories.Add(Kit.Layout.Data("Client"));
        StepOutcome outcome = Engine(world).Uninstall(true);
        bool gone = SetupLayout.ServiceFolders.All(name => !world.Directories.Contains(Kit.Layout.Data(name)));
        Check(outcome.Ok && gone, "uninstall with Remove data: the seven data folders are removed");
        Check(world.Directories.Contains(Kit.Layout.Data("Client")), "uninstall with Remove data: the Client folder stays");
        Check(!world.Files.ContainsKey(Kit.Layout.LogFile), "uninstall with Remove data: the log does not bring the folder back");
    }

    private static void UninstallFailure()
    {
        FakeWorld world = Installed();
        world.FailOn = "stop";
        StepOutcome outcome = Engine(world).Uninstall(false);
        Check(!outcome.Ok && outcome.Message.Contains("stop_service") && world.Directories.Contains(Kit.Layout.ProgramDir), "uninstall: a failed step stops it with a clear message");
    }

    private static void UninstallLockedFolder()
    {
        FakeWorld world = Installed();
        world.LockedDeletes = 2;
        MaintenanceEngine engine = Engine(world);
        StepOutcome outcome = engine.Uninstall(false);
        Check(world.Calls.FindIndex(call => call.StartsWith("cwd ")) < world.Calls.FindIndex(call => call.StartsWith("deletedir ")), "uninstall: the current directory is moved before the programs are deleted");
        Check(outcome.Ok && !engine.ProgramFolderAtReboot && world.Calls.Count(call => call == "wait 1000") == 2, "uninstall: a locked folder is retried 1 s apart and then deleted");
        FakeWorld stuck = Installed();
        stuck.LockedDeletes = 99;
        MaintenanceEngine stuckEngine = Engine(stuck);
        StepOutcome last = stuckEngine.Uninstall(false);
        Check(last.Ok && stuckEngine.ProgramFolderAtReboot && stuck.Calls.Contains("atreboot " + Kit.Layout.ProgramDir), "uninstall: a folder locked 4 times is removed at the next restart and the uninstall finishes");
        Check(stuck.Calls.Count(call => call == "wait 1000") == 3 && Kit.LogWords(stuck).Contains("delete_programs at_reboot"), "uninstall: 3 retries and the at_reboot log line");
        FakeWorld failing = Installed();
        failing.FailOn = "close_client";
        StepOutcome failed = Engine(failing).Uninstall(false);
        Check(!failed.Ok && failed.Message.Contains("Done before it: Stopping the service, Deleting the service, Deleting the autostart task.") && !failed.Message.Contains("Nothing more was done"), "uninstall: the failure message lists the done steps");
    }

    private static void UninstallRelaunchDirectory()
    {
        FakeWorld world = new();
        int code = UninstallRelaunch.Start(world.Services, Kit.Package, @"P:\Temp");
        Check(code == 0 && world.DetachedDirectory != null && world.DetachedDirectory.StartsWith(@"P:\Temp\"), "uninstall relaunch: the temporary copy starts in the temporary folder");
    }

    private static void SettingsEmptyKey()
    {
        FakeWorld world = Installed();
        string keyBefore = world.Files[Kit.Layout.KeyFile];
        StepOutcome outcome = Engine(world).SaveSettings("https://new.example.org", "", false, "desktop", true);
        RunCall configure = world.Runs.Last();
        Check(outcome.Ok && configure.Input == "url=https://new.example.org\n", "settings: an empty key sends no key line, so the stored key stays");
        Check(world.Files[Kit.Layout.KeyFile] == keyBefore, "settings: the stored key is unchanged");
        string ini = world.Files[Kit.Layout.IniFile];
        Check(IniEditor.Get(ini, "device_type") == "desktop" && IniEditor.Get(ini, "server_enabled") == "0", "settings: device type and send switch are written");
        Check(world.Calls.IndexOf("stop") < world.Calls.LastIndexOf("start") && world.ServiceRunning, "settings: the service is restarted");
    }

    private static void SettingsWithKey()
    {
        FakeWorld world = Installed();
        StepOutcome outcome = Engine(world).SaveSettings("", Kit.Key, true, "laptop", true);
        Check(outcome.Ok && world.Runs.Last().Input == "key=" + Kit.Key + "\n", "settings: a new key goes through standard input only");
        Check(!Kit.LogText(world).Contains(Kit.Key), "settings: the key is not in the log");
    }

    private static void SettingsFailureRestartsService()
    {
        FakeWorld world = Installed();
        world.FailOn = "configure";
        StepOutcome outcome = Engine(world).SaveSettings("https://x.example.org", "", true, "laptop", true);
        Check(!outcome.Ok && world.ServiceRunning, "settings: when saving fails the service that was running is started again");
    }

    private static void SettingsAutostart()
    {
        FakeWorld world = Installed();
        Engine(world).SaveSettings("", "", true, "desktop", false);
        Check(world.Tasks.Count == 0, "settings: unticking autostart removes the task");
        Engine(world).SaveSettings("", "", true, "desktop", true);
        Check(world.Tasks.ContainsKey("EcoWattSense Client") && world.Tasks["EcoWattSense Client"].Contains("DeviceType=desktop"), "settings: ticking autostart registers the task named EcoWattSense Client");
    }

    private static void LineBreakCheck()
    {
        FakeWorld world = new();
        ConfigWriter writer = new(world.Services, Kit.Layout);
        int code = writer.Write("https://a.example.org\nkey=other", "", true, "laptop");
        Check(code == 87 && world.Runs.Count == 0, "settings: a value with a line break is refused before anything runs");
    }
}




/// <summary>Checks of the calibration result lines and of the off-thread run.</summary>
public static class CalibrationTextChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Check(CalibrationResult.TextFor(0) == "Calibration finished." && CalibrationResult.TextFor(1) == "Calibration stopped.", "calibration text: 0 and 1");
        Check(CalibrationResult.TextFor(2).StartsWith("Calibration needs the battery: unplug the charger.") && CalibrationResult.TextFor(3) == "Calibration was aborted.", "calibration text: 2 and 3");
        Check(CalibrationResult.TextFor(4) == "The model file could not be written." && CalibrationResult.TextFor(9) == "Calibration ended with code 9.", "calibration text: 4 and other");
        RunOffThread();
    }

    private static void RunOffThread()
    {
        FakeWorld world = new();
        InstallEngine engine = new(world.Services, Kit.Layout, new SetupLog(world.Services, Kit.Layout));
        engine.Prepare(Kit.Package);
        int code = Task.Run(engine.Calibrate).GetAwaiter().GetResult();
        Check(code == 7 && CalibrationResult.TextFor(code) == "Calibration ended with code 7.", "calibration: the run works from a worker thread and gives its text");
    }
}
