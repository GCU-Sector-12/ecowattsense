// DeveloperModeFile.cs: the per-user switch "Developer mode" (spec D2, section 2.1).
// Who uses it: the Settings page (the check box) and the sidebar (the "Developer" item) through MainWindow.Developer.cs.
// The switch is the file %LOCALAPPDATA%\EcoWattSense\developer-mode.txt. The text 1 means on; no file means off. Any
// other content, an unreadable file or a file over 16 bytes means off. Nothing else is ever written by this class.
#nullable enable
using System;
using System.IO;
using System.Text;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Reads and writes the developer mode file.</summary>
public static class DeveloperModeFile
{
    private const long MaxBytes = 16;

    /// <summary>The file of the current user.</summary>
    public static string DefaultPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EcoWattSense", "developer-mode.txt");

    /// <summary>Tells whether developer mode is on.</summary>
    /// <param name="path">The file path.</param>
    /// <returns>True only when the file holds the text 1 (spaces, line ends and a byte order mark are ignored).</returns>
    public static bool IsOn(string path)
    {
        byte[]? bytes = SafeFile.TryReadBytes(path, MaxBytes);
        if (bytes is null) return false;
        return Encoding.UTF8.GetString(bytes).Trim('﻿', ' ', '\t', '\r', '\n') == "1";
    }

    /// <summary>Switches developer mode on or off.</summary>
    /// <param name="path">The file path.</param>
    /// <param name="on">True writes the file, false removes it.</param>
    /// <returns>True when the file is now as asked. False when it could not be written or removed.</returns>
    public static bool TrySet(string path, bool on)
    {
        try { return on ? Write(path) : Remove(path); }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException) { return false; }
    }

    /// <summary>Writes the text 1, and makes the folder when it is missing.</summary>
    /// <param name="path">The file path.</param>
    /// <returns>Always true.</returns>
    private static bool Write(string path)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path) ?? ".");
        File.WriteAllText(path, "1");
        return true;
    }

    /// <summary>Deletes the file. A missing file is fine.</summary>
    /// <param name="path">The file path.</param>
    /// <returns>Always true.</returns>
    private static bool Remove(string path)
    {
        File.Delete(path);
        return true;
    }
}
