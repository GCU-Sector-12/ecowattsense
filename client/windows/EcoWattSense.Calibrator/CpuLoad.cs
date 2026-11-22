// CpuLoad.cs: busy threads that load the processor in the "CPU 50%" and "CPU 100%" phases.
// Who uses it: TestRunner starts it at the beginning of a CPU phase and stops it at the end (and in the final clean-up).
// How it works: each worker is a dedicated thread at below-normal priority that spins on maths until a CancellationToken is cancelled.
// Below-normal priority keeps the window and the sampling responsive while the processor is fully loaded.
namespace EnergyCalibrator;

/// <summary>Starts and stops busy-loop threads.</summary>
internal sealed class CpuLoad
{
    private readonly List<Thread> threads = new();
    private CancellationTokenSource? stop;

    /// <summary>Written by the busy loops so that the compiler cannot remove their work. Nobody reads it.</summary>
    internal static double Sink;

    /// <summary>Starts busy threads. Any earlier threads are stopped first.</summary>
    /// <param name="threadCount">Number of busy threads.</param>
    public void Start(int threadCount)
    {
        Stop();
        stop = new CancellationTokenSource();
        for (int i = 0; i < threadCount; i++) { threads.Add(StartWorker(stop.Token, i)); }
    }

    /// <summary>Stops all busy threads and waits up to 2 seconds for each. Safe to call when nothing runs.</summary>
    public void Stop()
    {
        stop?.Cancel();
        foreach (Thread thread in threads) { thread.Join(TimeSpan.FromSeconds(2)); }
        threads.Clear();
        stop?.Dispose();
        stop = null;
    }

    /// <summary>Creates and starts one busy thread.</summary>
    /// <param name="token">Cancelled to stop the thread.</param>
    /// <param name="index">Number of the thread, for its name.</param>
    /// <returns>The running thread.</returns>
    private static Thread StartWorker(CancellationToken token, int index)
    {
        Thread thread = new(() => Spin(token)) { IsBackground = true, Priority = ThreadPriority.BelowNormal, Name = $"CalibrationLoad{index}" };
        thread.Start();
        return thread;
    }

    /// <summary>Runs bursts of maths until the token is cancelled. The token is checked after every burst (about a millisecond).</summary>
    /// <param name="token">The stop token.</param>
    private static void Spin(CancellationToken token)
    {
        double value = 1.5;
        while (!token.IsCancellationRequested) { value = Burst(value); }
        Sink = value;
    }

    /// <summary>One burst of floating-point maths.</summary>
    /// <param name="seed">The value carried from the last burst, so the work cannot be skipped.</param>
    /// <returns>The new value; it always stays between about 0.7 and 2.5.</returns>
    private static double Burst(double seed)
    {
        double value = seed;
        for (int i = 0; i < 20_000; i++) { value = Math.Sqrt((value * 1.0000001) + 1.0) + (Math.Sin(value) * 0.5); }
        return value;
    }
}
