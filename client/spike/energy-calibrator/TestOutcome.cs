// TestOutcome.cs: why a run stopped (AbortReason), the event data of a phase (PhaseEventArgs) and everything a run produced (TestOutcome).
// Who uses it: TestRunner and MainWindow create and read them; PhaseAnalyser, ServiceModel and ResultWriter work from them.
// No window code here, so the console tests compile it.
namespace EnergyCalibrator;

/// <summary>Why a run stopped before the last phase ended.</summary>
internal enum AbortReason
{
    /// <summary>The run was not stopped.</summary>
    None,
    /// <summary>The user pressed Stop or closed the window.</summary>
    UserStop,
    /// <summary>The charger was plugged in.</summary>
    Charger,
    /// <summary>The battery fell below the limit.</summary>
    BatteryLow,
    /// <summary>The mouse or keyboard was used while the display was off.</summary>
    UserInput,
    /// <summary>An error stopped the run.</summary>
    Failure,
    /// <summary>The display did not report "off" within the time limit after the command.</summary>
    DisplayStayedOn,
    /// <summary>The display reported "on" or "dimmed" while the Screen off phase ran.</summary>
    DisplayWoke
}

/// <summary>Tells the window that a phase started or ended.</summary>
/// <param name="index">Index of the phase.</param>
/// <param name="status">Running when it started, the final status when it ended.</param>
internal sealed class PhaseEventArgs(int index, PhaseStatus status) : EventArgs
{
    /// <summary>Index of the phase.</summary>
    public int Index { get; } = index;

    /// <summary>The status of the phase.</summary>
    public PhaseStatus Status { get; } = status;
}

/// <summary>Everything a run produced.</summary>
/// <param name="StartedAt">Wall-clock time when the run started.</param>
/// <param name="Samples">Every sample taken during the run.</param>
/// <param name="Statuses">The final status of every phase.</param>
/// <param name="Notes">A note for every phase (empty when none), for example a brightness that could not be set.</param>
/// <param name="Reason">Why the run stopped early, or None.</param>
/// <param name="ReasonText">The reason as a sentence.</param>
/// <param name="RestoreProblems">Things that could not be put back at the end.</param>
internal sealed record TestOutcome(
    DateTime StartedAt,
    IReadOnlyList<Sample> Samples,
    IReadOnlyList<PhaseStatus> Statuses,
    IReadOnlyList<string> Notes,
    AbortReason Reason,
    string ReasonText,
    IReadOnlyList<string> RestoreProblems);
