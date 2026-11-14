// ProgressChecks.cs: tests of the install progress and of the Finish button (SPEC section 10).
// Run by scripts\Test-Setup.ps1 through Program.Main. The collector below stands in for the window: it keeps every report
// in order, so a test can read what the person would have seen.
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>Keeps every install report in the order it arrived.</summary>
public sealed class ReportList : IProgress<InstallProgress>
{
    /// <summary>The reports so far.</summary>
    public List<InstallProgress> Items { get; } = [];

    /// <summary>Stores one report.</summary>
    public void Report(InstallProgress value) => Items.Add(value);

    /// <summary>The names of the steps that reported the given state, in order.</summary>
    public string[] Names(StepState state) => Items.Where(item => item.State == state).Select(item => item.Name).ToArray();
}

/// <summary>The checks of the progress reports and of starting the client.</summary>
public static class ProgressChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        OrderChecks();
        CopyChecks();
        FailedChecks();
        FinishChecks();
    }

    private static InstallEngine NewEngine(FakeWorld world)
    {
        InstallEngine engine = new(world.Services, Kit.Layout, new SetupLog(world.Services, Kit.Layout));
        engine.Prepare(Kit.Package);
        return engine;
    }

    private static void OrderChecks()
    {
        FakeWorld world = new();
        ReportList reports = new();
        StepOutcome outcome = NewEngine(world).Install(new InstallRequest(Kit.Package, "", "", false), reports);
        string[] expected = InstallTitles.Order.Take(8).ToArray();
        Check(outcome.Ok && reports.Names(StepState.Done).SequenceEqual(expected), "progress: every step reports done, in order");
        Check(reports.Names(StepState.Working).Distinct().SequenceEqual(expected), "progress: every step reports working before it is done");
        Check(reports.Items.First().State == StepState.Working && reports.Items.First().Text == "Stopping the old service", "progress: the first report is the plain title of the first step");
    }

    private static void CopyChecks()
    {
        FakeWorld world = new() { CopyFileCount = 5 };
        ReportList reports = new();
        NewEngine(world).Install(new InstallRequest(Kit.Package, "", "", false), reports);
        InstallProgress[] counts = reports.Items.Where(item => item.Total > 0).ToArray();
        Check(counts.Length == 5 && counts.Select(item => item.Done).SequenceEqual([1, 2, 3, 4, 5]), "progress: the copy reports each file");
        Check(counts[1].Total == 5 && counts[1].Text == "Copying programs 2 of 5", "progress: the copy text holds the count");
    }

    private static void FailedChecks()
    {
        FakeWorld world = new() { FailOn = "register" };
        ReportList reports = new();
        StepOutcome outcome = NewEngine(world).Install(new InstallRequest(Kit.Package, "", "", false), reports);
        InstallProgress last = reports.Items.Last();
        Check(!outcome.Ok && last.Name == "register_service" && last.State == StepState.Failed && last.Text == outcome.Message, "progress: a failed step is marked failed with the message");
        Check(!reports.Names(StepState.Working).Contains("start_service"), "progress: no step after the failed one starts");
    }

    private static void FinishChecks()
    {
        FakeWorld ticked = new();
        InstallEngine engine = NewEngine(ticked);
        engine.Install(new InstallRequest(Kit.Package, "", "", false));
        int code = engine.Finish(true);
        Check(code == 0 && ticked.Calls.Contains(@"client P:\Prog\EcoWattSense.Client.Gui.exe"), "finish: the client starts from the program folder when ticked");
        Check(Kit.LogWords(ticked).Last() == "start_client started", "finish: the start is logged");
        FakeWorld unticked = new();
        InstallEngine other = NewEngine(unticked);
        other.Install(new InstallRequest(Kit.Package, "", "", false));
        Check(other.Finish(false) == 0 && !unticked.Calls.Any(call => call.StartsWith("client ")), "finish: the client does not start when unticked");
        FakeWorld failing = new() { ClientStartExit = -1 };
        InstallEngine third = NewEngine(failing);
        StepOutcome outcome = third.Install(new InstallRequest(Kit.Package, "", "", false));
        Check(third.Finish(true) == -1 && outcome.Ok && failing.ServiceRunning, "finish: a start failure returns the code and does not undo the install");
        Check(Kit.LogWords(failing).Last() == "start_client failed", "finish: a start failure is logged as start_client failed");
    }
}
