using System.Configuration;
using System.Data;
using System.Windows;

namespace EcoWattSense.Client.Gui;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        const string hiddenSwitch = "--start-hidden";
        var hostArgs = e.Args.Where(arg => !arg.Equals(hiddenSwitch, StringComparison.OrdinalIgnoreCase)).ToArray();
        bool startHidden = hostArgs.Length != e.Args.Length;
        var window = new MainWindow();
        MainWindow = window;
        if (!startHidden) window.Show();
        window.StartMonitoring(hostArgs, startHidden);
    }
}
