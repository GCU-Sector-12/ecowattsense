// PowerSettingsSession.cs: changes the processor power settings for one test and puts them back exactly as they were.
// Who uses it: MainWindow creates one PowerSettingsSession for each run (and one at program start to recover from a crash);
// TestRunner calls Restore in its clean-up path; ResultWriter writes the PowerRunRecord into summary.txt and model.json.
// How it works: Begin reads the settings, saves them to power-settings-backup.json next to the program, and only then writes the
// test values. Restore writes the saved values back, reads them again to check, and deletes the backup file only when they match.
// If the program is killed during a test, the backup file stays; the next start restores from it (PowerSettingsSession.Recover).
// Restore may be called many times: after a successful restore, or when nothing was changed, it does nothing.
using System.IO;
using System.Text.Json;

namespace EnergyCalibrator;

/// <summary>The backup file with the original power settings. It exists only while the settings are changed.</summary>
internal static class PowerSettingsBackup
{
    private const string FileName = "power-settings-backup.json";
    private static readonly JsonSerializerOptions Options = new() { WriteIndented = true };

    /// <summary>Gives the path of the backup file, next to the program.</summary>
    /// <returns>The path.</returns>
    public static string FilePath() => Path.Combine(AppContext.BaseDirectory, FileName);

    /// <summary>Writes the backup file.</summary>
    /// <param name="snapshot">The original settings.</param>
    /// <exception cref="IOException">The file cannot be written.</exception>
    public static void Save(PowerSnapshot snapshot)
    {
        File.WriteAllText(FilePath(), JsonSerializer.Serialize(snapshot, Options));
    }

    /// <summary>Tells whether a backup file exists.</summary>
    /// <returns>True when it exists.</returns>
    public static bool Exists() => File.Exists(FilePath());

    /// <summary>Reads the backup file.</summary>
    /// <returns>The saved settings, or null when the file holds no usable settings.</returns>
    /// <exception cref="IOException">The file cannot be read.</exception>
    public static PowerSnapshot? Load()
    {
        return JsonSerializer.Deserialize<PowerSnapshot>(File.ReadAllText(FilePath()));
    }

    /// <summary>Deletes the backup file. Does nothing when there is none.</summary>
    /// <exception cref="IOException">The file cannot be deleted.</exception>
    public static void Delete()
    {
        File.Delete(FilePath());
    }
}

/// <summary>Changes the power settings for one test and restores them. It never throws; problems are kept in the record.</summary>
internal sealed class PowerSettingsSession
{
    private PowerSnapshot? original;
    private bool changed;

    /// <summary>What happened to the settings in this session.</summary>
    public PowerRunRecord Record { get; } = new();

    /// <summary>
    /// Saves the original settings to the backup file and then sets maximum performance on battery. If anything fails, what was
    /// written is put back, and the record says why the test runs with unchanged settings. Call it on a pool thread.
    /// </summary>
    /// <returns>The record.</returns>
    public PowerRunRecord Begin()
    {
        try { BeginCore(); }
        catch (Exception ex) { FailBegin(Reasons.From(ex)); }
        return Record;
    }

    /// <summary>
    /// Restores the settings from the backup file of an earlier run that did not end cleanly (the program crashed or was killed).
    /// Call it once at program start, on a pool thread.
    /// </summary>
    /// <returns>A sentence for the status line, or an empty string when there was no backup file.</returns>
    public string Recover()
    {
        try { return RecoverCore(); }
        catch (Exception ex) { return $"The power settings of an earlier test could not be put back ({Reasons.From(ex)}). The backup file is kept: {PowerSettingsBackup.FilePath()}"; }
    }

    /// <summary>
    /// Puts the original settings back, reads them again and compares. The backup file is deleted only after the values match.
    /// It does nothing when nothing was changed or when the settings were already put back. Call it on a pool thread.
    /// </summary>
    /// <returns>The record.</returns>
    public PowerRunRecord Restore()
    {
        if (!changed || original == null) { return Record; }
        try { RestoreCore(original); }
        catch (Exception ex) { Record.RestoreProblem = Reasons.From(ex); }
        return Record;
    }

    /// <summary>Reads, saves a backup, and writes the test values.</summary>
    private void BeginCore()
    {
        PowerSnapshot before = PowerSettingsReader.ReadActive();
        Record.Before = before;
        original = before;
        // The backup comes first: no setting is written unless the file that can undo it is on disk.
        PowerSettingsBackup.Save(before);
        changed = true;
        PowerSettingsWriter.ApplyTestValues(before);
        Record.During = before.ForTest();
    }

    /// <summary>Records why the change failed and puts back whatever was written.</summary>
    /// <param name="reason">The reason, as a short sentence.</param>
    private void FailBegin(string reason)
    {
        Record.ChangeProblem = reason;
        Record.During = null;
        Restore();
    }

    /// <summary>Restores from the backup file.</summary>
    /// <returns>The sentence for the status line.</returns>
    private string RecoverCore()
    {
        if (!PowerSettingsBackup.Exists()) { return ""; }
        original = PowerSettingsBackup.Load();
        if (original == null) { return $"The backup file of the power settings holds nothing usable and was kept: {PowerSettingsBackup.FilePath()}"; }
        changed = true;
        Restore();
        if (Record.RestoreVerified) { return "The processor power settings of an earlier test that did not finish were put back."; }
        return $"The power settings of an earlier test could not be put back ({Record.RestoreProblem}). The backup file is kept: {PowerSettingsBackup.FilePath()}";
    }

    /// <summary>Writes the original values, checks them and removes the backup file.</summary>
    /// <param name="target">The original settings.</param>
    private void RestoreCore(PowerSnapshot target)
    {
        Record.RestoreProblem = "";
        PowerSettingsWriter.PutBack(target);
        PowerSnapshot now = PowerSettingsReader.ReadScheme(target.Scheme);
        Record.After = now;
        Record.RestoreVerified = target.SameValues(now);
        if (!Record.RestoreVerified) { throw new SourceUnavailableException("the values read back after the restore differ from the original ones"); }
        // Only now is it safe to forget the original values.
        PowerSettingsBackup.Delete();
        changed = false;
    }
}

/// <summary>Builds the warning that the user must accept before the test starts.</summary>
internal static class StartWarning
{
    /// <summary>Builds the text of the confirmation box.</summary>
    /// <param name="fullChargeMwh">Full charge capacity of the battery in mWh, or null when it cannot be read.</param>
    /// <returns>The text, one paragraph per line.</returns>
    public static string Build(double? fullChargeMwh)
    {
        double minutes = InstructionTexts.TotalMinutes();
        double usedMwh = minutes / 60.0 * TestSettings.FullPowerEstimateWatts * 1000.0;
        return string.Join(Environment.NewLine + Environment.NewLine,
            $"The test runs for about {Fmt.Number(minutes, "0")} minutes on the battery and drains it at full power. {Estimate(usedMwh, fullChargeMwh)}",
            "For the test, the processor power settings for battery use are set to maximum performance. They are put back as they were when the test ends, is stopped or fails. If the program is killed, they are put back the next time it starts.",
            "Do not plug in the charger and do not close the laptop lid until the test is over.",
            "Press OK to start, or Cancel to leave everything as it is.");
    }

    /// <summary>Estimates how much of the battery the test uses.</summary>
    /// <param name="usedMwh">Energy of the test at the assumed power, in mWh.</param>
    /// <param name="fullChargeMwh">Full charge capacity in mWh, or null.</param>
    /// <returns>A sentence with "about" and a rounded number.</returns>
    private static string Estimate(double usedMwh, double? fullChargeMwh)
    {
        string watts = Fmt.WithUnit(TestSettings.FullPowerEstimateWatts, "0", "W");
        string usedWh = Fmt.WithUnit(usedMwh / 1000.0, "0.0", "Wh");
        if (fullChargeMwh is not double full) { return $"That is about {usedWh} at about {watts}; the battery size could not be read."; }
        double percent = Math.Min(100, Math.Round(usedMwh / full * 100.0));
        string fullWh = Fmt.WithUnit(full / 1000.0, "0.0", "Wh");
        return $"It is expected to use about {Fmt.Number(percent, "0")}% of the battery ({usedWh} at about {watts}, against a full charge of {fullWh}).";
    }
}
