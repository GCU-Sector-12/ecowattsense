// AutostartTask.cs: reads and changes the "start the client with Windows" task through the existing scripts.
// Who uses it: the check box on the Settings page (MainWindow.Settings.cs) and Test-SettingsInfo.ps1 (the argument lists).
// The task is the per-user Task Scheduler task of Install-Autostart.ps1 (no administrator rights, run level Limited).
// Changing it runs Install-Autostart.ps1 or Uninstall-Autostart.ps1 with Windows PowerShell; reading it asks schtasks.
// Setup.exe can register the same task for every user (a group principal, GroupId in the task XML). Read() tells the three
// cases apart (no task, a user task, an all-users task) so the Settings page can lock the box when Setup owns the task.
// Every call blocks until the program ends, so the window calls them from a pool thread.
#nullable enable
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Xml.Linq;

namespace EcoWattSense.Client.Gui.Settings;

/// <summary>Who the autostart task runs for.</summary>
public enum AutostartKind
{
    /// <summary>The answer is not clear (schtasks failed, or the XML could not be read).</summary>
    Unknown,
    /// <summary>No task exists.</summary>
    None,
    /// <summary>The per-user task of Install-Autostart.ps1.</summary>
    User,
    /// <summary>The task of Setup.exe: a group principal, so it runs for every user.</summary>
    AllUsers
}

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

    /// <summary>Asks Task Scheduler for the task and tells who it runs for.</summary>
    /// <returns>None when the task does not exist, Unknown when schtasks fails, otherwise what Parse says.</returns>
    public static AutostartKind Read()
    {
        int code = RunCaptured("schtasks.exe", new[] { "/Query", "/TN", TaskName, "/XML" }, out string xml);
        if (code == ExitTaskMissing) return AutostartKind.None;
        if (code != 0) return AutostartKind.Unknown;
        return Parse(xml);
    }

    /// <summary>Tells from the task XML whether the task has a group principal (Setup, all users) or a user principal.</summary>
    /// <param name="xml">The output of schtasks /query /xml, or null or empty when there is no task.</param>
    /// <returns>None for an empty text, AllUsers when the Principal has a GroupId, User when it has none, Unknown when the XML is broken.</returns>
    public static AutostartKind Parse(string? xml)
    {
        if (string.IsNullOrWhiteSpace(xml)) return AutostartKind.None;
        try { return ParseDocument(xml); }
        catch (System.Xml.XmlException) { return AutostartKind.Unknown; }
    }

    /// <summary>Looks for GroupId inside the Principal element. Names are compared without the namespace, which Task Scheduler sets.</summary>
    /// <param name="xml">The task XML.</param>
    /// <returns>AllUsers or User.</returns>
    /// <exception cref="System.Xml.XmlException">The text is not XML.</exception>
    private static AutostartKind ParseDocument(string xml)
    {
        XElement? principal = XDocument.Parse(xml).Descendants().FirstOrDefault(e => e.Name.LocalName == "Principal");
        bool group = principal is not null && principal.Elements().Any(e => e.Name.LocalName == "GroupId");
        return group ? AutostartKind.AllUsers : AutostartKind.User;
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
    private static int Run(string file, string[] arguments) => RunCaptured(file, arguments, out _);

    /// <summary>Runs a program with no window, waits for it to end and keeps what it printed.</summary>
    /// <param name="file">The program name or path.</param>
    /// <param name="arguments">The arguments, one item each.</param>
    /// <param name="output">The standard output text (empty when the program could not be started).</param>
    /// <returns>The exit code, or -1 when the program could not be started.</returns>
    private static int RunCaptured(string file, string[] arguments, out string output)
    {
        output = "";
        var info = new ProcessStartInfo(file) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
        foreach (string argument in arguments) info.ArgumentList.Add(argument);
        try { return RunStarted(info, out output); }
        catch (Win32Exception) { return -1; }
    }

    /// <summary>Starts the prepared process and waits for it.</summary>
    /// <param name="info">The start information.</param>
    /// <param name="output">The standard output text.</param>
    /// <returns>The exit code, or -1 when no process was created.</returns>
    /// <exception cref="Win32Exception">The program was not found or could not be started.</exception>
    private static int RunStarted(ProcessStartInfo info, out string output)
    {
        output = "";
        using Process? process = Process.Start(info);
        if (process is null) return -1;
        output = process.StandardOutput.ReadToEnd();
        process.StandardError.ReadToEnd();
        process.WaitForExit();
        return process.ExitCode;
    }
}
