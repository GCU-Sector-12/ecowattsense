// RunRecorder.cs: saves the data of a run while it is measured, so a run that is cut short (process killed, power loss)
// keeps everything measured so far.
// Who uses it: MainWindow creates one for each run. It appends a row to samples.csv after every sample and a row to
// phases.csv when a phase ends. Each append opens and closes the file, so the data is on disk at once.
// What it does not do: it never throws and never stops the test. A failed write comes back as a short text for the status line.
// The growing samples.csv has the fixed columns only; extra columns (DTT, thermal zones, Energy Meter) are added by the final
// save when the run ends normally. After a kill those extra values are not in the file.
using System.IO;
using System.Text;

namespace EnergyCalibrator;

/// <summary>Appends the rows of one run to its CSV files.</summary>
internal sealed class RunRecorder(string folder)
{
    private static readonly UTF8Encoding Utf8 = new(false);
    private bool samplesIncomplete;

    /// <summary>Writes the header rows of samples.csv and phases.csv.</summary>
    /// <returns>Null when both headers were written; otherwise a short reason for the status line.</returns>
    public string? Start()
    {
        string? samplesProblem = AppendLine("samples.csv", SamplesCsv.FixedHeader());
        string? phasesProblem = AppendLine("phases.csv", PhasesCsv.Header());
        samplesIncomplete = samplesProblem != null;
        return samplesProblem ?? phasesProblem;
    }

    /// <summary>Appends the row of one sample to samples.csv.</summary>
    /// <param name="sample">The sample, already stamped with its phase.</param>
    /// <returns>Null when the row was written; otherwise a short reason for the status line.</returns>
    public string? AppendSample(Sample sample)
    {
        string? problem = AppendLine("samples.csv", SamplesCsv.FixedRow(sample));
        samplesIncomplete |= problem != null;
        return problem;
    }

    /// <summary>Appends the row of one finished phase to phases.csv.</summary>
    /// <param name="result">The result of the phase.</param>
    /// <returns>Null when the row was written; otherwise a short reason for the status line.</returns>
    public string? AppendPhase(PhaseResult result)
    {
        return AppendLine("phases.csv", PhasesCsv.Row(result));
    }

    /// <summary>Tells whether the final save must write samples.csv again from the full list.</summary>
    /// <param name="samples">All samples of the run.</param>
    /// <returns>True when a row is missing from the growing file, or the full file has more columns.</returns>
    public bool SamplesNeedRewrite(IReadOnlyList<Sample> samples)
    {
        return samplesIncomplete || SamplesCsv.HasExtraColumns(samples);
    }

    /// <summary>Appends one line to a file in the run folder.</summary>
    /// <param name="fileName">The file name.</param>
    /// <param name="line">The line, without a line break.</param>
    /// <returns>Null on success; otherwise a short reason that holds only the error type.</returns>
    private string? AppendLine(string fileName, string line)
    {
        try
        {
            File.AppendAllText(Path.Combine(folder, fileName), line + Environment.NewLine, Utf8);
            return null;
        }
        catch (Exception ex)
        {
            return $"Data could not be saved to {fileName} ({ex.GetType().Name}); the test goes on.";
        }
    }
}
