// ClientCloseChecks.cs: checks of the step "Closing the client" (SPEC section 10), install and uninstall.
// Who uses it: Program.Main. Uses FakeWorld, so no real process is touched.
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>Running clients are closed before the programs are copied or deleted.</summary>
public static class ClientCloseChecks
{
    /// <summary>Runs every check of this group.</summary>
    public static void Run()
    {
        InstallCloses();
        UninstallCloses();
        InstallFails();
        UninstallFails();
    }

    private static FakeWorld Installed()
    {
        FakeWorld world = new();
        Kit.RunInstall(world, Kit.Url, Kit.Key);
        world.Calls.Clear();
        return world;
    }

    private static MaintenanceEngine Engine(FakeWorld world) => new(world.Services, Kit.Layout, new SetupLog(world.Services, Kit.Layout));

    private static void InstallCloses()
    {
        FakeWorld world = new() { ClientsRunning = 2 };
        StepOutcome outcome = Kit.RunInstall(world, "", "");
        Check(outcome.Ok && world.ClientsRunning == 0, "close client: an install closes the running clients");
        Check(world.Calls.IndexOf("close_client") < world.Calls.IndexOf("copydir"), "close client: the clients are closed before the programs are copied");
        Check(Kit.LogWords(world).Contains("close_client 2"), "close client: the count of closed processes is logged");
        Check(InstallTitles.For("close_client") == "Closing the client", "close client: the step has its own title");
    }

    private static void UninstallCloses()
    {
        FakeWorld world = Installed();
        world.ClientsRunning = 1;
        StepOutcome outcome = Engine(world).Uninstall(false);
        int programs = world.Calls.IndexOf("deletedir " + Kit.Layout.ProgramDir);
        Check(outcome.Ok && world.ClientsRunning == 0, "close client: an uninstall closes the running clients");
        Check(world.Calls.IndexOf("close_client") >= 0 && world.Calls.IndexOf("close_client") < programs, "close client: the clients are closed before the programs are deleted");
    }

    private static void InstallFails()
    {
        FakeWorld world = new() { FailOn = "close_client", ClientsRunning = 1 };
        StepOutcome outcome = Kit.RunInstall(world, "", "");
        Check(!outcome.Ok && outcome.Message.Contains("close_client"), "close client: a process that cannot be closed fails the install step with a clear message");
        Check(!world.Calls.Contains("copydir") && !world.Directories.Contains(Kit.Layout.ProgramDir), "close client: nothing is copied after the failure");
    }

    private static void UninstallFails()
    {
        FakeWorld world = Installed();
        world.FailOn = "close_client";
        StepOutcome outcome = Engine(world).Uninstall(false);
        Check(!outcome.Ok && outcome.Message.Contains("close_client") && world.Directories.Contains(Kit.Layout.ProgramDir), "close client: a process that cannot be closed fails the uninstall step and keeps the programs");
    }
}
