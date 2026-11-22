// DisplayStateKind.cs: the states of the display and their plain-English texts.
// Who uses it: DisplayStateMonitor (DisplayControl.cs) sets them, Sample.cs and samples.csv carry them. No Windows calls here, so the console tests compile it.
namespace EnergyCalibrator;


/// <summary>What the display is doing, as reported by Windows.</summary>
internal enum DisplayStateKind
{
    /// <summary>No notification has arrived yet, or the registration failed.</summary>
    Unknown,
    /// <summary>The display is off.</summary>
    Off,
    /// <summary>The display is on.</summary>
    On,
    /// <summary>The display is on and dimmed.</summary>
    Dimmed
}

/// <summary>Plain-English texts for the display states. They are also the values of the display_state column in samples.csv.</summary>
internal static class DisplayStateTexts
{
    /// <summary>Gives the short text for a display state.</summary>
    /// <param name="state">The state.</param>
    /// <returns>"off", "on", "dimmed" or "unknown".</returns>
    public static string Describe(DisplayStateKind state) => state switch
    {
        DisplayStateKind.Off => "off",
        DisplayStateKind.On => "on",
        DisplayStateKind.Dimmed => "dimmed",
        _ => "unknown"
    };
}
