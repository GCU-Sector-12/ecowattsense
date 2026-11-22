// StreamData.cs: sample lines and objects of the energy stream for the checks.
// Who uses it: the *Checks.cs files of this folder. The header text is the example of spec section 1.6 with an AMD laptop and a discrete card.
using System.Text;
using EnergyCalibrator;

namespace CalibratorTests;

/// <summary>Ready-made stream text and objects.</summary>
internal static class StreamData
{
    /// <summary>A good header line: two energy channels (package, one core), one NVML device, two adapters.</summary>
    public const string HeaderLine =
        "{\"watch\":1,\"period_ms\":2000,\"emi\":[{\"name\":\"RAPL_Package0_PKG\",\"kind\":\"cpu_package\",\"device\":0,\"index\":0},"
        + "{\"name\":\"RAPL_Package0_Core0_CORE\",\"kind\":\"cpu_core\",\"device\":0,\"index\":1}],"
        + "\"nvml\":[{\"name\":\"NVIDIA Test\",\"adapter\":1,\"power\":true,\"energy\":false}],"
        + "\"gpu_use\":[{\"adapter\":0,\"role\":\"power_saving\",\"vendor_id\":4098},{\"adapter\":1,\"role\":\"high_performance\",\"vendor_id\":4318}],"
        + "\"machine\":{\"cpu_vendor\":\"AuthenticAMD\",\"cpu_family\":25,\"cpu_model\":80,\"cpu_stepping\":0,\"physical_cores\":8},"
        + "\"battery\":{\"count\":1,\"design_mwh\":50000,\"full_mwh\":45000,\"cycle_count\":120},"
        + "\"display\":{\"displays\":2,\"external_displays\":1,\"refresh_hz\":60},\"extra_field\":{\"x\":[1,2]}}";

    /// <summary>A good sample line for the header above.</summary>
    public const string SampleLine = "{\"t_ms\":4000,\"emi_uj\":[123456,null],\"nvml_mw\":[15300],\"nvml_mj\":[null],\"gpu_pct\":[12.5,0.0]}";

    /// <summary>Parses the good header line.</summary>
    /// <returns>The header.</returns>
    public static WatchHeader Header()
    {
        WatchHeader? header = WatchParser.ParseHeader(HeaderLine, out _);
        return header ?? throw new InvalidOperationException("the good header line was refused");
    }

    /// <summary>Builds a header with the given channels and no NVML device and no adapter, and plain conditions.</summary>
    /// <param name="channels">The channels as (name, kind).</param>
    /// <returns>The header.</returns>
    public static WatchHeader HeaderOf(params (string Name, string Kind)[] channels)
    {
        List<WatchChannel> emi = channels.Select((c, i) => new WatchChannel(c.Name, c.Kind, 0, i)).ToList();
        return new WatchHeader(2000, emi, [], [], new WatchMachine(null, null, null, null), new WatchBattery(null, null, null), new WatchDisplay(null, null, null));
    }

    /// <summary>Builds a header with the given channels, one NVML device and two adapters.</summary>
    /// <param name="channels">The channels as (name, kind).</param>
    /// <returns>The header.</returns>
    public static WatchHeader HeaderWithGpu(params (string Name, string Kind)[] channels)
    {
        WatchHeader plain = HeaderOf(channels);
        return plain with { Nvml = [new WatchNvml("NVIDIA Test", 1, true, false)], GpuUse = [new WatchGpuUse(0, "power_saving", 4098), new WatchGpuUse(1, "high_performance", 4318)] };
    }

    /// <summary>Builds the samples of one phase: one sample every 2 seconds, from 40 s into the phase.</summary>
    /// <param name="phaseNumber">Number of the phase (1 to 7).</param>
    /// <param name="count">Number of samples.</param>
    /// <param name="line">Gives the stream line of sample i (null for "no line"). The time of the line is 2000 x i ms.</param>
    /// <returns>The samples.</returns>
    public static List<Sample> Samples(int phaseNumber, int count, Func<int, WatchSample?> line)
    {
        List<Sample> samples = new();
        for (int i = 0; i < count; i++) { samples.Add(OneSample(phaseNumber, i, line(i))); }
        return samples;
    }

    /// <summary>Builds one sample of a phase.</summary>
    /// <param name="phaseNumber">Number of the phase.</param>
    /// <param name="i">Position in the phase.</param>
    /// <param name="line">The stream line, or null.</param>
    /// <returns>The sample.</returns>
    private static Sample OneSample(int phaseNumber, int i, WatchSample? line)
    {
        return new Sample { PhaseNumber = phaseNumber, SecondsIntoPhase = 40 + 2 * i, MonotonicSeconds = 1000.0 * phaseNumber + 2 * i, Watch = line };
    }

    /// <summary>Builds a line with the given values.</summary>
    /// <param name="timeMs">The time in ms.</param>
    /// <param name="emiUj">The energy of each channel.</param>
    /// <param name="nvmlMw">The power of each NVML device in mW.</param>
    /// <param name="gpuPct">The use of each adapter.</param>
    /// <returns>The line.</returns>
    public static WatchSample Line(long timeMs, long?[] emiUj, double?[]? nvmlMw = null, double?[]? gpuPct = null)
    {
        return new WatchSample(timeMs, emiUj, nvmlMw ?? [], [], gpuPct ?? []);
    }

    /// <summary>Makes a text of the given number of characters (letter a).</summary>
    /// <param name="length">The length.</param>
    /// <returns>The text.</returns>
    public static string Letters(int length) => new string('a', length);

    /// <summary>Makes a reader over lines joined with line breaks.</summary>
    /// <param name="lines">The lines.</param>
    /// <returns>The reader.</returns>
    public static StringReader Reader(params string[] lines) => new(string.Join("\n", lines));

    /// <summary>Counts the bytes of a text in UTF-8.</summary>
    /// <param name="text">The text.</param>
    /// <returns>The byte count.</returns>
    public static int Bytes(string text) => Encoding.UTF8.GetByteCount(text);
}
