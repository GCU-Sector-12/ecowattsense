// CalibratorRunner.cs: starts the calibrator and tells when a calibrator process runs or ends.
// Who uses it: the viewer window (MainWindow.Calibrate.cs).
// How: Refresh looks for processes named EnergyCalibrator and subscribes to Process.Exited of each, so the end is an
// event and nothing polls. The calibrator is started as the same user (UseShellExecute false, no verb), never elevated.
// A process of another user or an elevated one may refuse the subscription; it still counts as running when Refresh sees it.
#nullable enable
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Watches and starts the calibrator program.</summary>
public sealed class CalibratorRunner : IDisposable
{
    private const string ProcessName = "EnergyCalibrator";

    private readonly Dictionary<int, Process> watched = new();
    private readonly object gate = new();

    /// <summary>Raised on a pool thread when a watched calibrator process has ended.</summary>
    public event Action? Ended;

    /// <summary>Looks for calibrator processes and starts watching new ones.</summary>
    /// <returns>True when at least one calibrator process runs now.</returns>
    public bool Refresh()
    {
        Process[] found = Process.GetProcessesByName(ProcessName);
        foreach (Process process in found) Watch(process);
        return found.Length > 0;
    }

    /// <summary>Starts the calibrator as the current user and begins to watch it.</summary>
    /// <param name="path">The full path of EnergyCalibrator.exe.</param>
    /// <returns>True when the process was created; false when Windows refused or the file is gone.</returns>
    public bool Start(string path)
    {
        var info = new ProcessStartInfo(path) { UseShellExecute = false, WorkingDirectory = Path.GetDirectoryName(path) ?? "" };
        try { Process.Start(info)?.Dispose(); }
        catch (Win32Exception) { return false; }
        Refresh();
        return true;
    }

    /// <summary>Stops watching and releases the process objects.</summary>
    public void Dispose()
    {
        Process[] all;
        lock (gate)
        {
            all = watched.Values.ToArray();
            watched.Clear();
        }
        foreach (Process process in all) process.Dispose();
    }

    /// <summary>Keeps a process object and subscribes to its end. A process that is already watched is released.</summary>
    /// <param name="process">A process from GetProcessesByName.</param>
    private void Watch(Process process)
    {
        bool added;
        lock (gate) added = watched.TryAdd(process.Id, process);
        if (!added) { process.Dispose(); return; }
        TryEnableEvents(process);
    }

    /// <summary>Turns on the Exited event of a process.</summary>
    /// <param name="process">The process to watch.</param>
    /// <remarks>An access denied error (an elevated calibrator) is ignored: Refresh still sees that process.</remarks>
    private void TryEnableEvents(Process process)
    {
        try
        {
            process.Exited += OnExited;
            process.EnableRaisingEvents = true;
        }
        catch (Win32Exception)
        {
            RemoveAndRelease(process.Id);
        }
    }

    /// <summary>Forgets an ended process and tells the window.</summary>
    /// <param name="sender">The process that ended.</param>
    /// <param name="e">Not used.</param>
    private void OnExited(object? sender, EventArgs e)
    {
        if (sender is Process process) RemoveAndRelease(process.Id);
        Ended?.Invoke();
    }

    /// <summary>Removes a process from the watch list and disposes it.</summary>
    /// <param name="id">The process id.</param>
    private void RemoveAndRelease(int id)
    {
        Process? process;
        lock (gate) watched.Remove(id, out process);
        process?.Dispose();
    }
}
