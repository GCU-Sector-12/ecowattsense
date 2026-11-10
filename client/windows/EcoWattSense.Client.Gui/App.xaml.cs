// Starts the Windows monitoring GUI for normal and tray-only launches.

﻿using System.Configuration;
using System.Data;
using System.Windows;

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
        window.StartMonitoring(hostArgs, startHidden);
    }
}
