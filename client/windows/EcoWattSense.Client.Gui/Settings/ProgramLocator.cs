// ProgramLocator.cs: finds the companion programs of the client (calibrator, Setup, autostart scripts).
// Who uses it: the Calibrate button, the Change... button, the autostart check box and Test-ProgramLocator.ps1.
// Order: the client's own folder first (a portable unzip), then %ProgramFiles%\EcoWattSense\ (the Setup install).
// The existence check is passed in, so the order can be tested without touching the disk.
#nullable enable
using System;
using System.IO;

namespace EcoWattSense.Client.Gui.Settings;

/// <summary>Looks for a companion program next to the client or in the install folder.</summary>
public static class ProgramLocator
{
    /// <summary>The file name of the calibrator.</summary>
    public const string CalibratorName = "EnergyCalibrator.exe";

    /// <summary>The file name of Setup.</summary>
    public const string SetupName = "Setup.exe";

    /// <summary>The file name of the script that registers the autostart task.</summary>
    public const string InstallAutostartName = "Install-Autostart.ps1";

    /// <summary>The file name of the script that removes the autostart task.</summary>
    public const string UninstallAutostartName = "Uninstall-Autostart.ps1";

    /// <summary>Looks for a file in the two known places.</summary>
    /// <param name="fileName">The file name, for example "Setup.exe".</param>
    /// <param name="clientFolder">The folder of the client.</param>
    /// <param name="programFilesFolder">The Program Files folder; the file is looked for in its EcoWattSense subfolder.</param>
    /// <param name="exists">Tells whether a full path is an existing file.</param>
    /// <returns>The full path, or null when the file is in neither place.</returns>
    public static string? Find(string fileName, string clientFolder, string programFilesFolder, Func<string, bool> exists)
    {
        string own = Path.Combine(clientFolder, fileName);
        if (exists(own)) return own;
        string installed = Path.Combine(programFilesFolder, "EcoWattSense", fileName);
        return exists(installed) ? installed : null;
    }

    /// <summary>Looks for a file with the real folders of this computer.</summary>
    /// <param name="fileName">The file name.</param>
    /// <returns>The full path, or null when not found.</returns>
    public static string? FindHere(string fileName)
        => Find(fileName, AppContext.BaseDirectory, Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), File.Exists);

    /// <summary>Looks for the calibrator.</summary>
    /// <returns>The full path of EnergyCalibrator.exe, or null.</returns>
    public static string? FindCalibrator() => FindHere(CalibratorName);

    /// <summary>Looks for Setup.</summary>
    /// <returns>The full path of Setup.exe, or null.</returns>
    public static string? FindSetup() => FindHere(SetupName);
}
