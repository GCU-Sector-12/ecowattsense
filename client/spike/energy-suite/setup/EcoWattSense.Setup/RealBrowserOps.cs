// RealBrowserOps.cs: opens a web address in the default browser (IBrowserOps).
// Who uses it: SetupWindow gives it to ConnectClient, which opens the Connect page of the dashboard. The tests use a fake.
// Known limit (SPEC section 10): Setup runs elevated, so the browser may start as another Windows user who has no dashboard
// session. Then the admin opens the address in a browser where they are signed in.
using System.Diagnostics;

namespace EcoWattSense.Setup;

/// <summary>Opens an address through the shell, so the default browser of the user is used.</summary>
public sealed class RealBrowserOps : IBrowserOps
{
    /// <summary>Opens the address and does not wait. A browser that cannot be started is not an error for Setup.</summary>
    /// <param name="url">The full address.</param>
    /// <returns>0, or -1 when no browser could be started.</returns>
    public int Open(string url)
    {
        try
        {
            Process.Start(new ProcessStartInfo(url) { UseShellExecute = true })?.Dispose();
            return 0;
        }
        catch (Exception)
        {
            return -1;
        }
    }
}
