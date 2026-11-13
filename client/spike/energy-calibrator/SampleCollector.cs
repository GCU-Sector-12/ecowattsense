// SampleCollector.cs: reads every source into one Sample (SampleCollector) and does it every 2 seconds (SamplingLoop).
// Who uses it: MainWindow starts one SamplingLoop when the window opens and keeps it running; TestRunner listens to it.
// How it works: the reading runs on a thread-pool thread, because WMI calls can be slow; the event is raised back on the
// UI thread. Each source is guarded: a failure marks only that source as "not available" and the others still run.
namespace EnergyCalibrator;

/// <summary>Reads all sources once. Never throws: a failed source is marked unavailable with its reason.</summary>
internal sealed class SampleCollector
{
    private readonly BatteryIoctlReader batteryIoctl = new();
    private readonly PowerStatusReader powerStatus = new();
    private readonly WmiBatteryReader wmiBattery = new();
    private readonly DttReader dtt = new();
    private readonly CpuTimesReader cpuTimes = new();
    private readonly UserInputReader userInput = new();
    private readonly CounterReaders counters = new();
    private readonly BrightnessControl brightness = new();
    private readonly EnergyMeterReader energyMeter = new();

    /// <summary>Takes one sample of all sources. Call it from one thread at a time.</summary>
    /// <returns>The sample, with unknown values missing and failed sources listed.</returns>
    public Sample Collect()
    {
        Sample sample = new() { DisplayState = DisplayStateMonitor.Current };
        Guard(SampleKeys.SourceBatteryIoctl, batteryIoctl.Read, sample);
        Guard(SampleKeys.SourcePowerStatus, powerStatus.Read, sample);
        Guard(SampleKeys.SourceWmiBattery, wmiBattery.Read, sample);
        Guard(SampleKeys.SourceDtt, dtt.Read, sample);
        Guard(SampleKeys.SourceCpuTimes, cpuTimes.Read, sample);
        Guard(SampleKeys.SourceUserInput, userInput.Read, sample);
        Guard(SampleKeys.SourceCpuPerformance, counters.ReadCpuPerformance, sample);
        Guard(SampleKeys.SourceCpuFrequency, counters.ReadCpuFrequency, sample);
        Guard(SampleKeys.SourceThermal, counters.ReadThermal, sample);
        Guard(SampleKeys.SourceGpu, counters.ReadGpu, sample);
        Guard(SampleKeys.SourceDisk, counters.ReadDisk, sample);
        Guard(SampleKeys.SourceNetwork, counters.ReadNetwork, sample);
        Guard(SampleKeys.SourceBrightness, brightness.Read, sample);
        Guard(SampleKeys.SourceEnergyMeter, energyMeter.Read, sample);
        Guard(SampleKeys.SourcePowerSettings, PowerSettingsReader.Read, sample);
        return sample;
    }

    /// <summary>Runs one reader and turns any exception into an "unavailable" mark.</summary>
    /// <param name="source">Name of the source, for the mark.</param>
    /// <param name="read">The reader method.</param>
    /// <param name="sample">The sample to fill.</param>
    private static void Guard(string source, Action<Sample> read, Sample sample)
    {
        try { read(sample); }
        catch (Exception ex) { sample.MarkUnavailable(source, Reasons.From(ex)); }
    }
}

/// <summary>Takes a sample every SampleIntervalSeconds and raises SampleReady on the thread that called Start (the UI thread).</summary>
internal sealed class SamplingLoop
{
    private readonly SampleCollector collector = new();
    private readonly CancellationTokenSource cancel = new();

    /// <summary>Raised on the UI thread for every new sample. Handlers must not throw.</summary>
    public event EventHandler<Sample>? SampleReady;

    /// <summary>Starts the loop. Call it on the UI thread. The first sample is taken at once.</summary>
    public void Start()
    {
        _ = RunAsync();
    }

    /// <summary>Stops the loop after the sample that is being read.</summary>
    public void Stop()
    {
        cancel.Cancel();
    }

    /// <summary>The loop: one sample at once, then one for every tick of a periodic timer (no drift, no extra threads).</summary>
    private async Task RunAsync()
    {
        using PeriodicTimer timer = new(TimeSpan.FromSeconds(TestSettings.SampleIntervalSeconds));
        await PublishOnceAsync();
        while (await NextTickAsync(timer)) { await PublishOnceAsync(); }
    }

    /// <summary>Waits for the next tick.</summary>
    /// <param name="timer">The periodic timer.</param>
    /// <returns>True for a tick, false when the loop was stopped.</returns>
    private async Task<bool> NextTickAsync(PeriodicTimer timer)
    {
        try { return await timer.WaitForNextTickAsync(cancel.Token); }
        catch (OperationCanceledException) { return false; }
    }

    /// <summary>Reads a sample on a pool thread and raises the event back on the calling context.</summary>
    private async Task PublishOnceAsync()
    {
        Sample sample = await Task.Run(collector.Collect);
        SampleReady?.Invoke(this, sample);
    }
}
