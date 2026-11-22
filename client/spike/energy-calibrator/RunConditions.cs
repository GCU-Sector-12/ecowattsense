// RunConditions.cs: the "conditions" block of model.json (spec 2.4): what the laptop was like during the test, taken from the stream header.
// Who uses it: PowerModel.Build creates it; ServiceModel and ResultWriter write it. The service does not read it; it is for people who compare runs.
namespace EnergyCalibrator;

/// <summary>The conditions of one run.</summary>
internal sealed class RunConditions
{
    /// <summary>The conditions of a run without the stream: only the source of the energy values.</summary>
    public static RunConditions CounterSet { get; } = new() { Values = new Dictionary<string, object> { ["emi_source"] = "counter_set" } };

    /// <summary>The values by field name, in the order of the file. Unknown values are not in it.</summary>
    public IReadOnlyDictionary<string, object> Values { get; init; } = new Dictionary<string, object>();

    /// <summary>The warning words that the conditions cause.</summary>
    public IReadOnlyList<string> Warnings { get; init; } = [];

    /// <summary>Warning word: a second display was connected, so the screen numbers describe the built-in panel only.</summary>
    public const string ExternalDisplayWord = "external_display_connected";

    /// <summary>Builds the conditions.</summary>
    /// <param name="header">The stream header, or null when the calibrator read the counter set.</param>
    /// <returns>The conditions. Unknown values are left out.</returns>
    public static RunConditions From(WatchHeader? header)
    {
        if (header == null) { return CounterSet; }
        Dictionary<string, object> values = new();
        Add(values, "cpu_vendor", header.Machine.CpuVendor);
        Add(values, "cpu_family", header.Machine.CpuFamily);
        Add(values, "cpu_model", header.Machine.CpuModel);
        Add(values, "physical_cores", header.Machine.PhysicalCores);
        Add(values, "battery_design_mwh", header.Battery.DesignMwh);
        Add(values, "battery_full_mwh", header.Battery.FullMwh);
        Add(values, "battery_cycle_count", header.Battery.CycleCount);
        Add(values, "displays", header.Display.Displays);
        Add(values, "external_displays", header.Display.ExternalDisplays);
        Add(values, "refresh_hz", header.Display.RefreshHz);
        values["emi_source"] = "stream";
        return new RunConditions { Values = values, Warnings = header.Display.ExternalDisplays > 0 ? [ExternalDisplayWord] : [] };
    }

    /// <summary>Adds a value unless it is unknown.</summary>
    /// <param name="values">The values so far.</param>
    /// <param name="name">The field name.</param>
    /// <param name="value">The value, or null when unknown (a nullable number that has no value is null here).</param>
    private static void Add(Dictionary<string, object> values, string name, object? value)
    {
        if (value == null) { return; }
        values[name] = value;
    }
}
