// SwapChecks.cs: checks of the safe program copy (SPEC section 10): Setup waits for the service process to exit and swaps
// the program folder (copy to .new, rename the old folder to .old, rename .new into place, delete .old).
// Every check uses FakeWorld, so nothing on the real computer is read or changed.
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>Checks of the service stop wait and of the folder swap, including their undo.</summary>
public static class SwapChecks
{
    private const string New = @"P:\Prog.new";
    private const string Old = @"P:\Prog.old";
    private const string Current = @"P:\Prog";

    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        WaitForExitChecks();
        StuckProcessChecks();
        SwapOrderChecks();
        FreshInstallChecks();
        LeftoverChecks();
        LockedCurrentChecks();
        LockedOldChecks();
        UndoAfterLockedOldChecks();
        CopyFailureChecks();
    }

    private static void WaitForExitChecks()
    {
        FakeWorld world = Kit.OldInstall();
        world.ServicePid = 4242;
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        List<string> calls = world.Calls;
        bool ordered = calls.IndexOf("service.pid") < calls.IndexOf("stop") && calls.IndexOf("stop") < calls.IndexOf("waitexit 4242") && calls.IndexOf("waitexit 4242") < calls.IndexOf("copydir");
        Check(outcome.Ok && ordered, "stop wait: the process id is read, the service stopped, the process awaited, then the copy starts");
    }

    private static void StuckProcessChecks()
    {
        FakeWorld world = Kit.OldInstall();
        world.ServicePid = 4242;
        world.ProcessStaysAlive = true;
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        Check(!outcome.Ok && outcome.Message.Contains("did not stop"), "stop wait: a process that does not exit fails the step with a clear message");
        Check(!world.Calls.Contains("copydir") && world.ServiceRunning, "stop wait: nothing is copied and the old service runs again");
    }

    private static void SwapOrderChecks()
    {
        FakeWorld world = Kit.OldInstall();
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        string[] moves = [$"movedir {Current} -> {Old}", $"movedir {New} -> {Current}"];
        List<string> seen = world.Calls.Where(call => call.StartsWith("movedir")).ToList();
        Check(outcome.Ok && seen.SequenceEqual(moves), "swap: the old folder is renamed to .old, then .new takes its place");
        Check(world.Directories.Contains(Current) && !world.Directories.Contains(New) && !world.Directories.Contains(Old), "swap: only the new program folder is left");
        Check(world.Calls.Contains($"deletedir {Old}"), "swap: the old folder is deleted at the end");
        List<string> words = Kit.LogWords(world);
        Check(words.IndexOf("copy_new ok") >= 0 && words.IndexOf("copy_new ok") < words.IndexOf("swap ok") && words.IndexOf("swap ok") < words.IndexOf("remove_old ok"), "swap: copy_new, swap and remove_old are logged in order");
    }

    private static void FreshInstallChecks()
    {
        FakeWorld world = new();
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        List<string> seen = world.Calls.Where(call => call.StartsWith("movedir")).ToList();
        Check(outcome.Ok && seen.SequenceEqual([$"movedir {New} -> {Current}"]), "fresh swap: .new is renamed, there is no old folder to move");
        Check(world.Directories.Contains(Current) && !world.Directories.Contains(New), "fresh swap: the program folder exists and .new is gone");
    }

    private static void LeftoverChecks()
    {
        FakeWorld world = Kit.OldInstall();
        world.Directories.Add(New);
        world.Directories.Add(Old);
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        List<string> calls = world.Calls;
        Check(outcome.Ok && calls.IndexOf($"deletedir {New}") >= 0 && calls.IndexOf($"deletedir {New}") < calls.IndexOf("copydir"), "leftover: a .new from an earlier run is deleted before the copy");
        Check(calls.IndexOf($"deletedir {Old}") >= 0 && calls.IndexOf($"deletedir {Old}") < calls.IndexOf($"movedir {Current} -> {Old}"), "leftover: a .old from an earlier run is deleted before the swap");
    }

    private static void LockedCurrentChecks()
    {
        FakeWorld world = Kit.OldInstall();
        world.LockedMoves = 100;
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        int attempts = world.Calls.Count(call => call == $"movedir {Current} -> {Old}");
        int pauses = world.Calls.Count(call => call == "wait 1000");
        Check(!outcome.Ok && attempts == 6 && pauses == 5, "locked folder: one try and 5 retries, 1 second apart, then the step fails");
        Check(world.Directories.Contains(Current) && !world.Directories.Contains(New) && !world.Directories.Contains(Old), "locked folder: the old folder stays and no .new is left");
        Check(world.ServiceRunning && Kit.LogWords(world).Contains("swap failed"), "locked folder: the service runs again and the failure is logged");
    }

    private static void LockedOldChecks()
    {
        FakeWorld world = Kit.OldInstall();
        world.LockedDeletes = 4;
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        Check(outcome.Ok && world.Calls.Contains($"atreboot {Old}"), "locked .old: it is marked for deletion at the next restart and the install still works");
        Check(Kit.LogWords(world).Contains("remove_old at_reboot"), "locked .old: the log says at_reboot");
    }

    private static void UndoAfterLockedOldChecks()
    {
        FakeWorld world = Kit.OldInstall();
        world.LockedDeletes = 4;
        world.FailOn = "acl";
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        string back = $"movedir {Old} -> {Current}";
        Check(!outcome.Ok && world.Calls.Contains(back), "undo: a kept .old is renamed back to the program folder");
        Check(world.Directories.Contains(Current) && !world.Directories.Contains(Old) && !world.Directories.Contains(New), "undo: only the program folder is left");
    }

    private static void CopyFailureChecks()
    {
        FakeWorld world = Kit.OldInstall();
        world.FailOn = "copydir";
        StepOutcome outcome = Kit.RunInstall(world, Kit.Url, Kit.Key);
        Check(!outcome.Ok && !world.Calls.Any(call => call.StartsWith("movedir")), "copy failure: the old folder is not touched");
        Check(world.Directories.Contains(Current) && !world.Directories.Contains(New), "copy failure: no .new is left");
    }
}
