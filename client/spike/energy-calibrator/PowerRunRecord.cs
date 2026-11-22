// PowerRunRecord.cs: what happened to the processor power settings in one run (before, during, after), in text and in model.json form.
// Who uses it: PowerSettingsSession fills it; ResultWriter writes it into summary.txt and model.json. No Windows calls here, so the console tests compile it.
namespace EnergyCalibrator;

/// <summary>What happened to the power settings in one run. It goes into summary.txt and model.json.</summary>
internal sealed class PowerRunRecord
{
    /// <summary>The settings read before anything was changed; null when they could not be read.</summary>
    public PowerSnapshot? Before { get; set; }

    /// <summary>The settings used during the test; null when the test ran with the settings unchanged.</summary>
    public PowerSnapshot? During { get; set; }

    /// <summary>The settings read back after the restore; null when no restore was needed or none was done.</summary>
    public PowerSnapshot? After { get; set; }

    /// <summary>True when the restored values were read back and are the same as the original ones.</summary>
    public bool RestoreVerified { get; set; }

    /// <summary>Why the settings could not be changed; empty when they were changed. The test then ran with the settings unchanged.</summary>
    public string ChangeProblem { get; set; } = "";

    /// <summary>Why the settings could not be put back; empty when there was no problem.</summary>
    public string RestoreProblem { get; set; } = "";

    /// <summary>Describes the three states in plain English, for summary.txt.</summary>
    /// <returns>The lines, starting with a title.</returns>
    public IReadOnlyList<string> DescribeLines()
    {
        return
        [
            "Power settings of the processor on battery",
            $"Before the test: {Text(Before, ChangeProblem.Length > 0 ? "not read" : "unknown")}",
            $"Used during the test: {DuringText()}",
            $"After the test: {AfterText()}"
        ];
    }

    /// <summary>Gives the warnings about the power settings for the Warnings section of summary.txt. Restore problems are reported by the runner.</summary>
    /// <returns>Zero or more sentences.</returns>
    public IReadOnlyList<string> Warnings()
    {
        List<string> warnings = new();
        if (ChangeProblem.Length > 0) { warnings.Add($"The power settings could not be changed ({ChangeProblem}); the test ran with the settings unchanged, so the CPU 100% phase may not show full power."); }
        if (OverlayWasNotDefault()) { warnings.Add($"The Windows power mode was \"{PowerNames.Overlay(Before?.Overlay)}\"; Windows can hold the processor back with it whatever the plan says."); }
        if (OverlayChanged()) { warnings.Add("The Windows power mode changed during the test."); }
        return warnings;
    }

    /// <summary>Gives the record for model.json.</summary>
    /// <returns>An object that the JSON serializer writes with snake_case names.</returns>
    public object ToDocument()
    {
        return new
        {
            changed_for_the_test = During != null,
            change_problem = ChangeProblem,
            before = Before?.ToDocument(),
            during = During?.ToDocument(),
            after = After?.ToDocument(),
            restored_and_checked = RestoreVerified,
            restore_problem = RestoreProblem
        };
    }

    /// <summary>Describes one snapshot, or the given text when there is none.</summary>
    /// <param name="snapshot">The snapshot, or null.</param>
    /// <param name="missing">The text for a missing snapshot.</param>
    /// <returns>The description.</returns>
    private static string Text(PowerSnapshot? snapshot, string missing) => snapshot?.Describe() ?? missing;

    /// <summary>Describes the settings used during the test.</summary>
    /// <returns>The values, or the reason why they were not changed.</returns>
    private string DuringText()
    {
        if (During != null) { return During.Describe(); }
        return ChangeProblem.Length > 0 ? $"unchanged (the change failed: {ChangeProblem})" : "unchanged";
    }

    /// <summary>Describes the settings after the test.</summary>
    /// <returns>Whether they were put back and checked.</returns>
    private string AfterText()
    {
        if (RestoreProblem.Length > 0) { return $"NOT put back ({RestoreProblem}); the backup file power-settings-backup.json was kept"; }
        if (RestoreVerified && After != null) { return $"put back and checked by reading them again: {After.Describe()}"; }
        return "nothing was changed, so nothing had to be put back";
    }

    /// <summary>Tells whether the power mode overlay before the test was not the default one.</summary>
    /// <returns>True when it was read and is not Balanced.</returns>
    private bool OverlayWasNotDefault() => Before?.Overlay is Guid overlay && overlay != Guid.Empty;

    /// <summary>Tells whether the power mode overlay differs between before and after the test.</summary>
    /// <returns>True when both were read and differ.</returns>
    private bool OverlayChanged() => Before?.Overlay is Guid before && After?.Overlay is Guid after && before != after;
}
