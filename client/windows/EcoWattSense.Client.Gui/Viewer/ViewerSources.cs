// ViewerSources.cs: reads the machine values the viewer needs from Windows and from the data folder.
// Who uses it: ServicePipeReporter (idle seconds, brightness) and the viewer window (power source, device id, paths).
// Every read catches its failure and gives null, so a denied WMI class, a missing file or a failed API call
// shows "Unknown" on a card and never stops the viewer.
#nullable enable
using System;
using System.IO;
using System.Linq;
using System.Management;
using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Monitoring;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Reads values from Windows and from the data folder.</summary>
public static class ViewerSources
{
    private const string BrightnessQuery = "SELECT Active, CurrentBrightness FROM WmiMonitorBrightness";
    private static readonly WindowsActivityMonitor Activity = new();
    private static readonly WindowsPowerMonitor Power = new();

    private static string DataFolder => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "EcoWattSense");

    /// <summary>The folder that holds status.json: &lt;data&gt;\status\ (SPEC 2.2).</summary>
    public static string StatusFolder => Path.Combine(DataFolder, "status");

    /// <summary>The folder that holds the daily history files: &lt;data&gt;\history\ (SPEC 2.2).</summary>
    public static string HistoryFolder => Path.Combine(DataFolder, "history");

    /// <summary>The full path of status.json.</summary>
    public static string StatusFile => Path.Combine(StatusFolder, "status.json");

    /// <summary>Reads the seconds since the last keyboard or mouse input in this session.</summary>
    /// <returns>The seconds, or null when Windows does not answer.</returns>
    public static double? ReadIdleSeconds()
    {
        try { return Activity.GetInactivitySeconds(); }
        catch (Exception exception) when (exception is InvalidOperationException or PlatformNotSupportedException)
        {
            return null;
        }
    }

    /// <summary>Reads the power source.</summary>
    /// <returns>True on battery, false on mains, null when unknown or when the query fails.</returns>
    public static bool? ReadOnBattery()
    {
        try { return Power.IsOnBattery(); }
        catch (Exception exception) when (exception is InvalidOperationException or PlatformNotSupportedException)
        {
            return null;
        }
    }

    /// <summary>Reads the text of device-id.txt.</summary>
    /// <returns>The file text, or null when the file is missing or cannot be read.</returns>
    public static string? ReadDeviceIdText()
    {
        try { return File.ReadAllText(Path.Combine(DeviceIdentityStore.ResolveDirectory(), DeviceIdentityStore.FileName)); }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidOperationException)
        {
            return null;
        }
    }

    /// <summary>Reads the brightness of the active display through WMI (root\wmi WmiMonitorBrightness), as the calibrator does.</summary>
    /// <returns>The brightness in percent (0 to 100), or null for a desktop without a display that reports it, or when WMI is denied.</returns>
    public static int? ReadBrightness()
    {
        try { return QueryBrightness(); }
        catch (Exception exception) when (exception is ManagementException or UnauthorizedAccessException or InvalidCastException)
        {
            return null;
        }
    }

    /// <summary>Runs the WMI query and picks the active display.</summary>
    /// <returns>The brightness in percent, or null when no active display reports one or the value is out of range.</returns>
    /// <exception cref="ManagementException">The class or namespace is missing, or the query failed.</exception>
    /// <exception cref="UnauthorizedAccessException">Access to the class is denied.</exception>
    private static int? QueryBrightness()
    {
        using var searcher = new ManagementObjectSearcher(@"root\wmi", BrightnessQuery);
        using ManagementObjectCollection rows = searcher.Get();
        ManagementBaseObject? active = rows.Cast<ManagementBaseObject>().FirstOrDefault(IsActive);
        return active is null ? null : ToPercent(active["CurrentBrightness"]);
    }

    /// <summary>Tells whether a display row is the active one.</summary>
    /// <param name="row">A WmiMonitorBrightness row.</param>
    /// <returns>True when the Active property is true.</returns>
    private static bool IsActive(ManagementBaseObject row) => row["Active"] is true;

    /// <summary>Converts a WMI value to a percent.</summary>
    /// <param name="value">The CurrentBrightness property (a byte).</param>
    /// <returns>The percent, or null when the value is not a number from 0 to 100.</returns>
    private static int? ToPercent(object? value)
    {
        if (value is not IConvertible number) return null;
        int percent = Convert.ToInt32(number, System.Globalization.CultureInfo.InvariantCulture);
        return percent is >= 0 and <= 100 ? percent : null;
    }
}
