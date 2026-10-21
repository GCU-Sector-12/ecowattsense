using System;
using System.Text.Json;

namespace EcoWattSense.Client.Reporting;

/// <summary>Outbound serialization with fixed options, independent of application-wide JSON settings.</summary>
public static class ReportJson
{
    // System.Text.Json defaults write enums as numbers, Guid as lowercase D format,
    // and UTC DateTime with a trailing Z. Do not add a string-enum converter or omit defaults:
    // zero states, zero measurements, and false booleans are required report fields.
    private static readonly JsonSerializerOptions Options = new();

    public static string Serialize(DeviceReport report)
    {
        ArgumentNullException.ThrowIfNull(report);
        return JsonSerializer.Serialize(report, Options);
    }
}
