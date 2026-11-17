// PhaseModels.cs: the seven test phases (PhaseCatalogue), what one phase is (PhaseDefinition), the kind of load it
// applies (PhaseLoad) and how a phase ended (PhaseStatus).
// Who uses it: TestRunner runs the phases, PhaseAnalyser and PowerModel refer to them by index, the window lists them.
// How to use: to add or change a phase, edit PhaseCatalogue.All; the phase index constants must follow the list order.
namespace EnergyCalibrator;

/// <summary>The load that a phase puts on the laptop besides the screen brightness.</summary>
internal enum PhaseLoad
{
    /// <summary>The laptop is left alone.</summary>
    None,
    /// <summary>Half of the logical processors run a busy loop.</summary>
    CpuHalf,
    /// <summary>All logical processors run a busy loop.</summary>
    CpuFull,
    /// <summary>A full-screen window with animated shapes loads the graphics chip.</summary>
    Graphics,
    /// <summary>The display is switched off.</summary>
    DisplayOff
}

/// <summary>How far a phase got and how it ended.</summary>
internal enum PhaseStatus
{
    /// <summary>The phase has not started.</summary>
    NotRun,
    /// <summary>The phase is running now.</summary>
    Running,
    /// <summary>The phase ran for its full length.</summary>
    Complete,
    /// <summary>The charger was plugged in during the phase, so its result is not valid.</summary>
    InvalidCharger,
    /// <summary>The mouse or keyboard was used while the display was off, so the phase is not valid.</summary>
    InvalidInput,
    /// <summary>The user pressed Stop.</summary>
    StoppedByUser,
    /// <summary>The test stopped because the battery fell below the limit.</summary>
    StoppedBatteryLow,
    /// <summary>An error stopped the test.</summary>
    Failed,
    /// <summary>The display did not report "off" after the command, so the phase is not valid.</summary>
    InvalidDisplayOn,
    /// <summary>The display reported "on" or "dimmed" while it should have been off, so the phase is not valid.</summary>
    InvalidDisplayWoke
}

/// <summary>One phase of the test.</summary>
/// <param name="Number">Position in the list, starting at 1.</param>
/// <param name="Name">Short name shown in the window and in the result files.</param>
/// <param name="Detail">One sentence that tells the user what the laptop does in this phase.</param>
/// <param name="BrightnessPercent">Screen brightness to set at the start, or null to leave it as it is.</param>
/// <param name="Load">The extra load of the phase.</param>
/// <param name="Seconds">Length of the phase in seconds.</param>
internal sealed record PhaseDefinition(int Number, string Name, string Detail, int? BrightnessPercent, PhaseLoad Load, int Seconds);

/// <summary>The fixed list of phases and the index of each one.</summary>
internal static class PhaseCatalogue
{
    /// <summary>Index of "Rest, lowest brightness (screen on)".</summary>
    public const int ScreenLow = 0;

    /// <summary>Index of "Rest, highest brightness".</summary>
    public const int ScreenHigh = 1;

    /// <summary>Index of "Rest, brightness 50%", the reference for the load phases.</summary>
    public const int ScreenReference = 2;

    /// <summary>Index of "CPU 50%".</summary>
    public const int CpuHalf = 3;

    /// <summary>Index of "CPU 100%".</summary>
    public const int CpuFull = 4;

    /// <summary>Index of "Graphics".</summary>
    public const int Graphics = 5;

    /// <summary>Index of "Screen off".</summary>
    public const int ScreenOff = 6;

    /// <summary>All phases in the order they run.</summary>
    public static IReadOnlyList<PhaseDefinition> All { get; } = new List<PhaseDefinition>
    {
        new(1, "Rest, lowest brightness (screen on)", "Idle laptop, lowest brightness. The screen is still lit.", TestSettings.ScreenLowPercent, PhaseLoad.None, TestSettings.PhaseSeconds),
        new(2, "Rest, highest brightness", "Idle laptop, highest brightness.", TestSettings.ScreenHighPercent, PhaseLoad.None, TestSettings.PhaseSeconds),
        new(3, "Rest, brightness 50%", "Idle laptop, screen brightness 50% (the reference).", TestSettings.ScreenReferencePercent, PhaseLoad.None, TestSettings.PhaseSeconds),
        new(4, "CPU 50%", "Half of the logical processors busy, brightness 50%.", TestSettings.ScreenReferencePercent, PhaseLoad.CpuHalf, TestSettings.PhaseSeconds),
        new(5, "CPU 100%", "All logical processors busy, brightness 50%.", TestSettings.ScreenReferencePercent, PhaseLoad.CpuFull, TestSettings.PhaseSeconds),
        new(6, "Graphics", "Full-screen animation on the graphics chip, brightness 50%.", TestSettings.ScreenReferencePercent, PhaseLoad.Graphics, TestSettings.PhaseSeconds),
        new(7, "Screen off", "Display switched off. Do not touch the mouse or keyboard.", null, PhaseLoad.DisplayOff, TestSettings.PhaseSeconds)
    };
}

/// <summary>Plain-English texts for the phase statuses.</summary>
internal static class PhaseStatusTexts
{
    /// <summary>Gives the short text that the window and the result files show for a status.</summary>
    /// <param name="status">The status to describe.</param>
    /// <returns>A short English text; "Unknown" for a value this program does not know.</returns>
    public static string Describe(PhaseStatus status) => status switch
    {
        PhaseStatus.NotRun => "Waiting",
        PhaseStatus.Running => "Running",
        PhaseStatus.Complete => "Done",
        PhaseStatus.InvalidCharger => "Invalid: charger",
        PhaseStatus.InvalidInput => "Invalid: input",
        PhaseStatus.StoppedByUser => "Stopped by user",
        PhaseStatus.StoppedBatteryLow => "Stopped: battery low",
        PhaseStatus.Failed => "Failed",
        PhaseStatus.InvalidDisplayOn => "Invalid: display stayed on",
        PhaseStatus.InvalidDisplayWoke => "Invalid: display woke up",
        _ => "Unknown"
    };
}
