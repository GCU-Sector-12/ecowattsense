// Program.cs: console tests of Setup (SPEC section 10). Run by scripts\Test-Setup.ps1.
// The exit code is the number of failed checks (0 means all passed). Every test uses FakeWorld, so nothing on the real
// computer is read or changed. The tests call the same engines as the Setup window.
using System.Xml.Linq;
using EcoWattSense.Setup;

namespace SetupTests;

/// <summary>Counts checks and prints a line for each failure.</summary>
public static class Harness
{
    /// <summary>Number of checks run.</summary>
    public static int Total { get; private set; }

    /// <summary>Number of failed checks.</summary>
    public static int Failed { get; private set; }

    /// <summary>Records one check.</summary>
    /// <param name="ok">The condition that must be true.</param>
    /// <param name="name">What is checked.</param>
    public static void Check(bool ok, string name)
    {
        Total++;
        Failed += ok ? 0 : 1;
        Console.WriteLine((ok ? "PASS " : "FAIL ") + name);
    }
}

/// <summary>Entry point.</summary>
public static class Program
{
    /// <summary>Runs every group of checks.</summary>
    public static int Main()
    {
        PureChecks.Run();
        InstallChecks.Run();
        UndoChecks.Run();
        MaintenanceChecks.Run();
        ConnectionChecks.Run();
        DeviceNameChecks.Run();
        NavChecks.Run();
        Console.WriteLine($"{Harness.Total - Harness.Failed} passed, {Harness.Failed} failed, {Harness.Total} checks");
        return Harness.Failed;
    }
}

/// <summary>Shared set-up code for the checks.</summary>
public static class Kit
{
    /// <summary>The test layout on fake drive P.</summary>
    public static readonly SetupLayout Layout = new(@"P:\Prog", @"P:\Data");

    /// <summary>The package folder Setup runs from in the tests.</summary>
    public const string Package = @"P:\Pkg";

    /// <summary>A secret value that must never reach a log or a command line.</summary>
    public const string Key = "SECRET-KEY-12345";

    /// <summary>A server address that must never reach the log.</summary>
    public const string Url = "https://secret.example.org";

    /// <summary>The service SID of the tests.</summary>
    public static string ServiceSidText => ServiceSid.Compute(SetupLayout.ServiceName);

    /// <summary>Runs Prepare and Install on a world.</summary>
    public static StepOutcome RunInstall(FakeWorld world, string url, string key)
    {
        SetupLog log = new(world.Services, Layout);
        InstallEngine engine = new(world.Services, Layout, log);
        StepOutcome prepared = engine.Prepare(Package);
        return prepared.Ok ? engine.Install(new InstallRequest(Package, url, key, true)) : prepared;
    }

    /// <summary>The words of the log lines, one per line: step and result.</summary>
    public static List<string> LogWords(FakeWorld world)
    {
        string text = world.Files.GetValueOrDefault(Layout.LogFile, "");
        return text.Split('\n', StringSplitOptions.RemoveEmptyEntries).Select(WordsOf).ToList();
    }

    private static string WordsOf(string line)
    {
        string[] parts = line.Split(' ');
        return parts[1] + " " + parts[2];
    }

    /// <summary>The whole log text.</summary>
    public static string LogText(FakeWorld world) => world.Files.GetValueOrDefault(Layout.LogFile, "");

    /// <summary>A world with an old test installation: service registered and running, program folder present.</summary>
    public static FakeWorld OldInstall()
    {
        FakeWorld world = new() { ServiceExists = true, ServiceRunning = true };
        world.Directories.Add(Layout.ProgramDir);
        return world;
    }
}
