// DeviceNameRename.cs: the rules and texts for renaming the computer from the client (Settings page, "Rename...").
// Who uses it: RenameDialog.xaml.cs (checks the typed name, builds the pipe message), MainWindow.Settings.cs
// (the status lines under the name) and Test-Rename.ps1.
// The service gets the name in the pipe message "EWS1 name=<base64 of the UTF-8 name>\n". The pipe is no UAC step:
// the service itself writes the name and sends it to the server.
#nullable enable
using System;
using System.Linq;
using System.Text;

namespace EcoWattSense.Client.Gui.Settings;

/// <summary>The result of checking a typed name.</summary>
/// <param name="Valid">True when the name may be sent.</param>
/// <param name="Name">The trimmed name; empty means "remove the name".</param>
/// <param name="Reason">A short line that says why the name is not valid; empty when it is valid.</param>
public sealed record DeviceNameCheck(bool Valid, string Name, string Reason);

/// <summary>Checks a typed computer name and builds the pipe message for it.</summary>
public static class DeviceNameRules
{
    /// <summary>The largest pipe message the service reads, in bytes.</summary>
    public const int MaxMessageBytes = 256;

    /// <summary>The start of the pipe message.</summary>
    public const string MessagePrefix = "EWS1 name=";

    /// <summary>Checks the text a person typed.</summary>
    /// <param name="typed">The text of the box; null counts as empty.</param>
    /// <returns>The check result. An empty name is valid (it removes the name).</returns>
    public static DeviceNameCheck Check(string? typed)
    {
        string name = (typed ?? "").Trim();
        int length = name.EnumerateRunes().Count();
        if (length > SettingsReader.MaxNameLength) return new DeviceNameCheck(false, name, "Use at most 60 characters.");
        if (name.EnumerateRunes().Any(Rune.IsControl)) return new DeviceNameCheck(false, name, "Remove the control characters.");
        if (Message(name) is null) return new DeviceNameCheck(false, name, "This name is too long to send.");
        return new DeviceNameCheck(true, name, "");
    }

    /// <summary>Builds the pipe message for a name that is already trimmed.</summary>
    /// <param name="name">The trimmed name; empty means "no name".</param>
    /// <returns>The message with a final line feed, or null when it would be longer than <see cref="MaxMessageBytes"/> bytes.</returns>
    public static string? Message(string name)
    {
        string encoded = Convert.ToBase64String(Encoding.UTF8.GetBytes(name));
        string message = MessagePrefix + encoded + "\n";
        return message.Length <= MaxMessageBytes ? message : null;
    }
}

/// <summary>The line under the computer name on the Settings page.</summary>
public static class DeviceNameLines
{
    /// <summary>Gives the line for the name sync state.</summary>
    /// <param name="deviceName">The name the service holds, or null when unknown (an older service).</param>
    /// <param name="serverName">The name the server keeps, or null when none or unknown.</param>
    /// <param name="sync">"ok", "pending", "failed", or null when unknown.</param>
    /// <returns>A short line, or an empty text when there is nothing to say.</returns>
    public static string Line(string? deviceName, string? serverName, string? sync)
    {
        if (sync == "pending") return "Waiting to send the name to the server.";
        if (sync == "failed") return "The server did not accept the name.";
        if (sync != "ok" || deviceName is null) return "";
        bool same = serverName is null ? deviceName.Length == 0 : deviceName == serverName;
        if (same) return "";
        return $"The server keeps the name «{serverName}». An admin can change it in the dashboard.";
    }
}
