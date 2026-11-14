// NavChecks.cs: tests of the wizard page rules (WizardNav): Back, sidebar clicks and the lock after the install.
// Run by scripts\Test-Setup.ps1 through Program.Main. No window is needed.
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>The checks of the wizard navigation.</summary>
public static class NavChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        BackChecks();
        SidebarChecks();
        LockChecks();
    }

    private static void BackChecks()
    {
        WizardNav nav = new();
        nav.Show(0);
        Check(!nav.CanGoBack && nav.Back() == 0, "nav: no Back on the first page");
        nav.Show(1);
        nav.Show(2);
        Check(nav.CanGoBack && nav.Back() == 1, "nav: Back from Install goes to Server");
        Check(nav.CanGoBack && nav.Back() == 0, "nav: Back from Server goes to Scan");
        Check(nav.Reached == 2, "nav: Back keeps the furthest page reached");
    }

    private static void SidebarChecks()
    {
        WizardNav nav = new();
        nav.Show(0);
        nav.Show(1);
        Check(nav.CanGoTo(0) && !nav.CanGoTo(1), "nav: a reached step may be clicked, the current one not");
        Check(!nav.CanGoTo(2) && !nav.CanGoTo(3), "nav: a step not reached yet is refused");
        nav.Show(2);
        nav.Show(0);
        Check(nav.CanGoTo(2) && nav.CanGoTo(1), "nav: after going back, steps already reached stay open");
    }

    private static void LockChecks()
    {
        WizardNav nav = new();
        nav.Show(2);
        nav.Lock();
        nav.Show(3);
        Check(!nav.CanGoBack && nav.Back() == 3, "nav: Back is off after the install has run");
        Check(!nav.CanGoTo(0) && !nav.CanGoTo(1) && !nav.CanGoTo(2), "nav: earlier steps are not clickable after the install has run");
    }
}
