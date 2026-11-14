// PhaseData.cs: reads a phases.csv of the calibrator back into PhaseResult objects, so the tests can run the real analysis on real data.
// Who uses it: Program.cs of the tests. A column that the file does not have (an older file) gives an unknown value.
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;
using EnergyCalibrator;

namespace CalibratorTests;

/// <summary>Loads phase results from a phases.csv file.</summary>
internal static class PhaseData
{
    // A comma that is outside quotes: an even number of quotes follows it. The phase names hold commas.
    private static readonly Regex OutsideQuotes = new(",(?=(?:[^\"]*\"[^\"]*\")*[^\"]*$)");

    /// <summary>Reads the file.</summary>
    /// <param name="path">Path of the phases.csv file.</param>
    /// <param name="withoutEmi">True to blank every Energy Meter column, as on a computer that has no Energy Meter.</param>
    /// <returns>One result per row, in file order (the phase order of the calibrator).</returns>
    /// <exception cref="IOException">The file cannot be read.</exception>
    public static IReadOnlyList<PhaseResult> Load(string path, bool withoutEmi)
    {
        string[] lines = File.ReadAllLines(path);
        string[] header = lines[0].Split(',');
        return lines.Skip(1).Where(line => line.Length > 0).Select((line, index) => Row(header, OutsideQuotes.Split(line), index, withoutEmi)).ToList();
    }

    /// <summary>Builds the result of one row.</summary>
    /// <param name="header">The column names.</param>
    /// <param name="fields">The fields of the row.</param>
    /// <param name="index">Index of the phase.</param>
    /// <param name="withoutEmi">True to blank the Energy Meter columns.</param>
    /// <returns>The phase result; its status is always Complete.</returns>
    private static PhaseResult Row(string[] header, string[] fields, int index, bool withoutEmi)
    {
        return new PhaseResult
        {
            Phase = PhaseCatalogue.All[index],
            Status = PhaseStatus.Complete,
            SampleCount = (int)(Num(header, fields, "samples_used") ?? 0),
            Seconds = Num(header, fields, "seconds"),
            DropMwh = Num(header, fields, "battery_drop_mwh"),
            IoctlWatts = Num(header, fields, "battery_w_ioctl"),
            WmiDropMwh = Num(header, fields, "wmi_drop_mwh"),
            WmiWatts = Num(header, fields, "battery_w_wmi"),
            MeanCpuPercent = Num(header, fields, "mean_cpu_util_pct"),
            MeanCpuPackageWatts = Num(header, fields, "mean_cpu_package_w_dtt"),
            MeanEmiPackageWatts = Emi(header, fields, "mean_emi_pkg_w", withoutEmi),
            MeanEmiCoresWatts = Emi(header, fields, "mean_emi_cores_w", withoutEmi),
            MeanEmiGpuWatts = Emi(header, fields, "mean_emi_gpu_w", withoutEmi),
            MeanEmiDramWatts = Emi(header, fields, "mean_emi_dram_w", withoutEmi),
            MeanBrightness = Num(header, fields, "mean_brightness_pct")
        };
    }

    /// <summary>Reads an Energy Meter column.</summary>
    /// <param name="header">The column names.</param>
    /// <param name="fields">The fields of the row.</param>
    /// <param name="name">The column.</param>
    /// <param name="blank">True to give an unknown value.</param>
    /// <returns>The number, or null.</returns>
    private static double? Emi(string[] header, string[] fields, string name, bool blank)
    {
        if (blank) { return null; }
        return Num(header, fields, name);
    }

    /// <summary>Reads one number.</summary>
    /// <param name="header">The column names.</param>
    /// <param name="fields">The fields of the row.</param>
    /// <param name="name">The column.</param>
    /// <returns>The number, or null when the column is missing or the field is empty.</returns>
    private static double? Num(string[] header, string[] fields, string name)
    {
        int at = Array.IndexOf(header, name);
        if (at < 0 || fields[at].Length == 0) { return null; }
        return double.Parse(fields[at], CultureInfo.InvariantCulture);
    }
}
