// ViewerText.cs: the words and short texts of the viewer cards (SPEC section 9).
// Who uses it: the viewer window (MainWindow.Viewer.cs) and Test-StatusReader.ps1.
// It is pure: values in, text out, so the texts can be tested without a window. Every value that came from a file
// or from another agent is range-checked here and shown as "Unknown" when it is not usable.
#nullable enable
using System;
using System.Globalization;
using System.Linq;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Builds the texts of the viewer cards.</summary>
public static class ViewerText
{
    /// <summary>The text of a value that is not known.</summary>
    public const string UnknownWord = "Unknown";

    /// <summary>Gives the level word.</summary>
    /// <param name="level">"L1" to "L4", or null.</param>
    /// <returns>The level, or "Unknown" for null.</returns>
    public static string LevelWord(string? level) => level ?? UnknownWord;

    /// <summary>Gives the one-line meaning of a measurement level (SPEC 3.4).</summary>
    /// <param name="level">"L1" to "L4", or null or any other word.</param>
    /// <returns>The meaning; for an unknown word a line that says no level has been reported.</returns>
    public static string LevelLine(string? level) => level switch
    {
        "L1" => "Whole computer measured",
        "L2" => "Processor and memory measured, the rest from calibration",
        "L3" => "Estimated from calibration",
        "L4" => "Not measured; the server estimates",
        _ => "No level reported yet"
    };

    /// <summary>Gives the Last report card text.</summary>
    /// <param name="status">The checked status.json content.</param>
    /// <param name="zone">The time zone for the shown time (local time in the app).</param>
    /// <returns>For example "14 Nov 10:05 (ok)"; "No report yet" without a report time; the result word is "unknown" when missing.</returns>
    public static string LastReport(StatusInfo status, TimeZoneInfo zone)
    {
        if (status.LastReportUtc is not { } reported) return "No report yet";
        DateTimeOffset local = TimeZoneInfo.ConvertTime(reported, zone);
        return $"{local.ToString("d MMM HH:mm", CultureInfo.InvariantCulture)} ({status.SendResult ?? "unknown"})";
    }

    /// <summary>Gives the Calibrated card text.</summary>
    /// <param name="status">The checked status.json content.</param>
    /// <param name="zone">The time zone for the shown date.</param>
    /// <returns>For example "12 Nov 2026", or "Not calibrated" when there is no model date.</returns>
    public static string Calibrated(StatusInfo status, TimeZoneInfo zone)
    {
        if (status.ModelGeneratedUtc is not { } made) return "Not calibrated";
        DateTimeOffset local = TimeZoneInfo.ConvertTime(made, zone);
        return local.ToString("d MMM yyyy", CultureInfo.InvariantCulture);
    }

    /// <summary>Gives the first 8 characters of the device id.</summary>
    /// <param name="fileText">The content of device-id.txt, or null when it could not be read.</param>
    /// <returns>The first 8 characters when the text starts with 8 hexadecimal digits, otherwise "Unknown".</returns>
    public static string ShortId(string? fileText)
    {
        string text = (fileText ?? "").Trim();
        if (text.Length < 8) return UnknownWord;
        string head = text.Substring(0, 8);
        return IsAllHex(head) ? head.ToLowerInvariant() : UnknownWord;
    }

    /// <summary>Tells whether every character is a hexadecimal digit.</summary>
    /// <param name="text">The text to check.</param>
    /// <returns>True when all characters are 0-9, a-f or A-F.</returns>
    private static bool IsAllHex(string text) => text.All(Uri.IsHexDigit);

    /// <summary>Gives the Power now text.</summary>
    /// <param name="watts">The power in watts, or null when unknown.</param>
    /// <returns>For example "12.3 W"; "Unknown" for null, a negative value, NaN or infinity.</returns>
    public static string Watts(double? watts)
        => watts is { } value && double.IsFinite(value) && value >= 0
            ? value.ToString("F1", CultureInfo.InvariantCulture) + " W"
            : UnknownWord;

    /// <summary>Gives the Today text.</summary>
    /// <param name="kwh">The energy in kilowatt hours, or null when unknown.</param>
    /// <returns>For example "0.42 kWh"; "Unknown" for null, a negative value, NaN or infinity.</returns>
    public static string Kilowatthours(double? kwh)
        => kwh is { } value && double.IsFinite(value) && value >= 0
            ? value.ToString("F2", CultureInfo.InvariantCulture) + " kWh"
            : UnknownWord;

    /// <summary>Gives the Power source text.</summary>
    /// <param name="onBattery">True on battery, false on mains, null when Windows does not say.</param>
    /// <returns>"Battery", "Mains" or "Unknown".</returns>
    public static string PowerSource(bool? onBattery) => onBattery switch
    {
        true => "Battery",
        false => "Mains",
        _ => UnknownWord
    };
}
