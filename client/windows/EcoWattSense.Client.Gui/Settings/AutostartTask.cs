// AutostartTask.cs: reads and changes the "start the client with Windows" task through the existing scripts.
// Who uses it: the check box on the Settings page (MainWindow.Settings.cs) and Test-SettingsInfo.ps1 (the argument lists).
// The task is the per-user Task Scheduler task of Install-Autostart.ps1 (no administrator rights, run level Limited).
// Changing it runs Install-Autostart.ps1 or Uninstall-Autostart.ps1 with Windows PowerShell; reading it asks schtasks.
// Every call blocks until the program ends, so the window calls them from a pool thread.
#nullable enable
using System;
using System.ComponentModel;
using System.Diagnostics;

namespace EcoWattSense.Client.Gui.Settings;

/// <summary>The autostart task of the client.</summary>
public static class AutostartTask
{
    /// <summary>The task name used by Install-Autostart.ps1 (its default).</summary>
    public const string TaskName = "EcoWattSense Client";

    private const int ExitTaskMissing = 1;

    /// <summary>Gives the argument list that registers the task.</summary>
    /// <param name="scriptPath">The full path of Install-Autostart.ps1.</param>
    /// <param name="deviceType">"laptop" or "desktop".</param>
    /// <param name="exePath">The full path of the client exe.</param>
    /// <returns>The arguments for powershell.exe, one item per argument (no quoting needed).</returns>
    public static string[] EnableArguments(string scriptPath, string deviceType, string exePath)
        => new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", scriptPath, "-DeviceType", deviceType, "-ExePath", exePath };

    /// <summary>Gives the argument list that removes the task.</summary>
    /// <param name="scriptPath">The full path of Uninstall-Autostart.ps1.</param>
    /// <returns>The arguments for powershell.exe.</returns>
    public static string[] DisableArguments(string scriptPath)
        => new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", scriptPath };

    /// <summary>Tells whether the task is registered.</summary>
    /// <returns>True when schtasks finds it, false when it does not exist, null when the answer is not clear.</returns>
    public static bool? IsRegistered()
    {
        int code = Run("schtasks.exe", new[] { "/Query", "/TN", TaskName });
        return code switch
        {
            0 => true,
            ExitTaskMissing => false,
            _ => null
        };
    }

    /// <summary>Registers the task for the current user.</summary>
    /// <param name="scriptPath">The full path of Install-Autostart.ps1.</param>
    /// <param name="deviceType">"laptop" or "desktop".</param>
    /// <param name="exePath">The full path of the client exe.</param>
    /// <returns>True when the script ended with exit code 0.</returns>
    public static bool Enable(string scriptPath, string deviceType, string exePath)
        => Run("powershell.exe", EnableArguments(scriptPath, deviceType, exePath)) == 0;

    /// <summary>Removes the task for the current user.</summary>
    /// <param name="scriptPath">The full path of Uninstall-Autostart.ps1.</param>
    /// <returns>True when the script ended with exit code 0.</returns>
    public static bool Disable(string scriptPath)
        => Run("powershell.exe", DisableArguments(scriptPath)) == 0;

    /// <summary>Runs a program with no window and waits for it to end.</summary>
    /// <param name="file">The program name or path.</param>
    /// <param name="arguments">The arguments, one item each.</param>
    /// <returns>The exit code, or -1 when the program could not be started.</returns>
    private static int Run(string file, string[] arguments)
    {
        var info = new ProcessStartInfo(file) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
        foreach (string argument in arguments) info.ArgumentList.Add(argument);
        try { return RunStarted(info); }
        catch (Win32Exception) { return -1; }
    }

    /// <summary>Starts the prepared process and waits for it.</summary>
    /// <param name="info">The start information.</param>
    /// <returns>The exit code, or -1 when no process was created.</returns>
    /// <exception cref="Win32Exception">The program was not found or could not be started.</exception>
    private static int RunStarted(ProcessStartInfo info)
    {
        using Process? process = Process.Start(info);
        if (process is null) return -1;
        process.StandardOutput.ReadToEnd();
        process.StandardError.ReadToEnd();
        process.WaitForExit();
        return process.ExitCode;
    }
}
