// Reasons.cs: turns an exception of a reader into a short reason that is safe to show and to save.
// Who uses it: SampleCollector and the other readers. It uses WMI types, so it is a Windows-only file and the console tests do not compile it.
using System.Management;

namespace EnergyCalibrator;

/// <summary>Turns exceptions into short reasons without any text from the computer (no names, paths or ids).</summary>
internal static class Reasons
{
    /// <summary>Gives a short reason for a failed read.</summary>
    /// <param name="ex">The exception that the read threw.</param>
    /// <returns>A plain-English reason. Unknown exceptions give only the exception type name.</returns>
    public static string From(Exception ex)
    {
        if (ex is SourceUnavailableException known) { return known.Message; }
        if (ex is UnauthorizedAccessException) { return "access denied (run as administrator)"; }
        if (ex is ManagementException wmi) { return FromWmi(wmi); }
        return $"read failed ({ex.GetType().Name})";
    }

    /// <summary>Gives a short reason for a WMI error, using only the WMI status name.</summary>
    /// <param name="ex">The WMI exception.</param>
    /// <returns>A plain-English reason.</returns>
    private static string FromWmi(ManagementException ex)
    {
        if (ex.ErrorCode == ManagementStatus.AccessDenied) { return "access denied (run as administrator)"; }
        if (ex.ErrorCode == ManagementStatus.InvalidClass) { return "not supported on this computer"; }
        if (ex.ErrorCode == ManagementStatus.NotFound) { return "not supported on this computer"; }
        return $"WMI error {ex.ErrorCode}";
    }
}
