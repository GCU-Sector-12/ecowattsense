// UpgradeChecks.cs: checks of the upgrade mode `Setup.exe /upgrade` (SPEC section 10) and of the version text.
// Every check uses FakeWorld, so nothing on the real computer is read or changed.
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>Checks of the upgrade: step order, untouched configuration, fallback and undo.</summary>
public static class UpgradeChecks
{
    private const string Version = "0.1.0";

    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        OrderChecks();
        ConfigChecks();
        FallbackChecks();
        UndoChecks();
        ClientChecks();
        VersionChecks();
    }

    private static FakeWorld InstalledWorld()
    {
        FakeWorld world = Kit.OldInstall();
        world.Files[Kit.Layout.IniFile] = "device_type=laptop\nserver_url=https://old.example\nserver_enabled=1\n";
        world.Files[Kit.Layout.KeyFile] = "protected";
        world.Files[Kit.Layout.DeviceNameFile] = "Kitchen";
        world.ClientsRunning = 1;
        return world;
    }

    private static StepOutcome RunUpgrade(FakeWorld world, List<InstallProgress>? reports = null)
    {
        SetupLog log = new(world.Services, Kit.Layout);
        InstallEngine engine = new(world.Services, Kit.Layout, log);
        IProgress<InstallProgress>? progress = reports == null ? null : new Collector(reports);
        return engine.Upgrade(Kit.Package, Version, progress);
    }

    /// <summary>A progress target that adds each report at once, so the tests need no UI thread.</summary>
    private sealed class Collector(List<InstallProgress> target) : IProgress<InstallProgress>
    {
        /// <inheritdoc />
        public void Report(InstallProgress value) => target.Add(value);
    }

    private static void OrderChecks()
    {
        FakeWorld world = InstalledWorld();
        List<InstallProgress> reports = [];
        StepOutcome outcome = RunUpgrade(world, reports);
        Check(outcome.Ok && outcome.Message == "Updated to 0.1.0.", "upgrade mode: a good run says Updated to <version>.");
        string[] expected = ["close_client 1", "close_client ok", "wait_service_exit ok", "stop_old_service ok", "copy_new ok", "swap ok", "remove_old ok", "copy_programs ok", "data_folders ok", "start_service ok", "autostart_task ok", "start_client started"];
        Check(Kit.LogWords(world).SequenceEqual(expected), "upgrade mode: the steps run and log in the order close, stop, copy, rights, start, task, client");
        string[] listed = reports.Where(report => report.State == StepState.Done).Select(report => report.Name).ToArray();
        Check(listed.SequenceEqual(InstallTitles.UpgradeOrder), "upgrade mode: progress reports the steps of the upgrade list in order");
        int stop = world.Calls.IndexOf("stop");
        Check(stop >= 0 && stop < world.Calls.IndexOf("copydir") && world.Calls.IndexOf("copydir") < world.Calls.IndexOf("acl") && world.Calls.IndexOf("acl") < world.Calls.IndexOf("start"), "upgrade mode: stop, copy, rights, start happen in that order");
        Check(world.ServiceRunning && world.Calls.Contains("client " + Kit.Layout.ClientExe), "upgrade mode: the service runs and the client is started at the end");
    }

    private static void ConfigChecks()
    {
        FakeWorld world = InstalledWorld();
        Dictionary<string, string> before = new(world.Files);
        RunUpgrade(world);
        Check(!world.Calls.Contains("configure") && world.Runs.Count == 0, "upgrade mode: --configure and the scanner are not run");
        Check(Kit.Layout.IniFile.Length > 0 && world.Files[Kit.Layout.IniFile] == before[Kit.Layout.IniFile], "upgrade mode: service.ini is untouched");
        Check(world.Files[Kit.Layout.KeyFile] == "protected" && world.Files[Kit.Layout.DeviceNameFile] == "Kitchen", "upgrade mode: the key and the computer name are untouched");
        Check(world.Tasks.TryGetValue(SetupLayout.TaskName, out string? xml) && xml.Contains("DeviceType=laptop"), "upgrade mode: the autostart task keeps the stored device type");
        Check(world.RegisterCount == 0 && !world.Calls.Contains("service.delete"), "upgrade mode: the service is neither registered again nor deleted");
    }

    private static void FallbackChecks()
    {
        FakeWorld none = new();
        SetupMode noService = InstallEngine.ChooseMode(SetupMode.Upgrade, none.Services, Kit.Layout);
        Check(noService == SetupMode.Install, "upgrade mode: falls back to the install pages without a service and a program folder");
        FakeWorld onlyService = new() { ServiceExists = true };
        Check(InstallEngine.ChooseMode(SetupMode.Upgrade, onlyService.Services, Kit.Layout) == SetupMode.Install, "upgrade mode: a service without a program folder is not an installation");
        FakeWorld onlyFolder = new();
        onlyFolder.Directories.Add(Kit.Layout.ProgramDir);
        Check(InstallEngine.ChooseMode(SetupMode.Upgrade, onlyFolder.Services, Kit.Layout) == SetupMode.Install, "upgrade mode: a program folder without a service is not an installation");
        Check(InstallEngine.ChooseMode(SetupMode.Upgrade, Kit.OldInstall().Services, Kit.Layout) == SetupMode.Upgrade, "upgrade mode: stays when an installation exists");
        Check(InstallEngine.ChooseMode(SetupMode.Settings, none.Services, Kit.Layout) == SetupMode.Settings, "upgrade mode: other modes are not changed");
        Check(SetupArguments.Parse(["/upgrade"]).Mode == SetupMode.Upgrade, "arguments: /upgrade");
    }

    private static void UndoChecks()
    {
        FakeWorld world = InstalledWorld();
        world.FailOn = "copydir";
        StepOutcome outcome = RunUpgrade(world);
        Check(!outcome.Ok && outcome.Message.Contains("copy_programs") && outcome.Message.Contains("upgrade"), "upgrade mode: a failed copy gives one clear message");
        Check(world.ServiceExists && world.ServiceRunning, "upgrade mode: after a failed copy the old service runs again");
        Check(world.Directories.Contains(Kit.Layout.ProgramDir), "upgrade mode: after a failed copy the old program folder stays");
        Check(!world.Calls.Any(call => call.StartsWith("client ")), "upgrade mode: after a failed copy the client is not started");
        Check(Kit.LogWords(world).Contains("stop_old_service undone"), "upgrade mode: the undo is logged");
    }

    private static void ClientChecks()
    {
        FakeWorld world = InstalledWorld();
        world.ClientStartExit = 5;
        StepOutcome outcome = RunUpgrade(world);
        Check(outcome.Ok && outcome.Note.Contains("could not be started"), "upgrade mode: a client that does not start is a note, not a failure");
    }

    private static void VersionChecks()
    {
        Check(SetupVersion.Trim("0.1.0+abc123") == "0.1.0" && SetupVersion.Trim("0.1.0") == "0.1.0", "version: the build metadata is cut off");
        Check(SetupVersion.Trim(null) == "0.0.0" && SetupVersion.Trim(" ") == "0.0.0", "version: an empty text gives 0.0.0");
    }
}

/// <summary>Checks of the /server=<url> argument.</summary>
public static class ServerArgChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Check(SetupArguments.Parse(["/server=https://dev.example.org"]).ServerUrl == "https://dev.example.org", "server arg: a good https address is read");
        Check(SetupArguments.Parse(["/SERVER=https://a.b", "/upgrade"]).ServerUrl == "https://a.b", "server arg: case does not matter and other arguments stay");
        Check(SetupArguments.Parse([]).ServerUrl == "", "server arg: none gives empty");
        string[] bad = ["http://a.b", "ftp://a.b", "https://", "https://a b", "https://a\"b", "https://a'b", "https://a\tb", "https://" + new string('a', 200), "a.b"];
        Check(bad.All(text => SetupArguments.Parse(["/server=" + text]).ServerUrl == ""), "server arg: a bad value is ignored");
        Check(ServerUrlRules.Clean("https://" + new string('a', 192)) != "", "server arg: 200 characters are allowed");
        Check(ServerUrlRules.Prefill("", "https://x.y") == "https://x.y", "server arg: prefills when no address is stored");
        Check(ServerUrlRules.Prefill("https://old.example", "https://x.y") == "https://old.example", "server arg: a stored address wins");
    }
}
