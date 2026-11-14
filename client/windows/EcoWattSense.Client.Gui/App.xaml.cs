// Starts normal and tray-only monitoring launches for the Windows GUI client.

using System.Configuration;
using System.Data;
using System.Windows;
using EcoWattSense.Client.Gui.Viewer;

namespace EcoWattSense.Client.Gui;

/// <summary>Manages startup for the Windows monitoring application.</summary>
public partial class App : Application
{
    /// <summary>Creates the main window and starts monitoring in the requested mode.</summary>
    /// <param name="e">The launch arguments, including the optional tray-only switch.</param>
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        const string hiddenSwitch = "--start-hidden";
        var hostArgs = e.Args.Where(arg => !arg.Equals(hiddenSwitch, StringComparison.OrdinalIgnoreCase)).ToArray();
        bool startHidden = hostArgs.Length != e.Args.Length;
        var window = new MainWindow();
        MainWindow = window;
        if (!startHidden) window.Show();
        // SPEC 9: with the service installed the client only shows data; it does not sample or report.
        if (ViewerMode.IsViewer(ScmServiceLookup.Query)) window.StartViewer();
        else window.StartMonitoring(hostArgs, startHidden);
    }
}
