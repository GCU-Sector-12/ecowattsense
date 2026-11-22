// WatchClient.cs: starts "ews-scan.exe --watch", reads its lines on a pool thread and keeps the latest one for the sampling loop.
// Who uses it: MainWindow starts it at the start of a test and stops it when the test ends, is stopped or the window closes.
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace EnergyCalibrator;

/// <summary>A running program that prints the stream. Tests give a fake one.</summary>
internal interface IWatchProcess : IDisposable
{
    /// <summary>Its standard output as text.</summary>
    TextReader Output { get; }

    /// <summary>Closes its standard input. The scanner ends when it sees that.</summary>
    void CloseInput();

    /// <summary>Waits for it to end.</summary>
    /// <param name="milliseconds">The longest wait.</param>
    /// <returns>True when it ended.</returns>
    bool WaitForExit(int milliseconds);

    /// <summary>Ends it at once.</summary>
    void Kill();
}

/// <summary>Starts the program of the stream.</summary>
internal interface IWatchLauncher
{
    /// <summary>Starts the program.</summary>
    /// <returns>The running program.</returns>
    /// <exception cref="IOException">It cannot be started (missing file, no permission).</exception>
    IWatchProcess Start();
}

/// <summary>
/// Reads the stream of one program on a pool thread, so the window never waits for it. It keeps the latest line for the sampling loop.
/// A failed start, a silent program, a bad header, a program that ends in the middle: each leaves "no data" (unknown), never an invented value.
/// </summary>
/// <param name="launcher">Starts the program.</param>
internal sealed class WatchClient(IWatchLauncher launcher)
{
    /// <summary>The longest time the calibrator waits for the header.</summary>
    public static readonly TimeSpan HeaderTimeout = TimeSpan.FromSeconds(5);

    private const int ExitWaitMs = 3000;
    private const int KillWaitMs = 2000;
    private const int ReadingWaitSeconds = 3;
    private const double StalePeriods = 3;
    private const double StaleExtraSeconds = 1;

    private readonly object gate = new();
    private readonly WatchSession session = new();
    private readonly TaskCompletionSource<WatchHeader?> headerReady = new(TaskCreationOptions.RunContinuationsAsynchronously);
    private readonly CancellationTokenSource cancel = new();
    private IWatchProcess? process;
    private Task reading = Task.CompletedTask;
    private Task? stopping;
    private bool stopped;

    /// <summary>The header; null until it arrived.</summary>
    public WatchHeader? Header
    {
        get { lock (gate) { return session.Header; } }
    }

    /// <summary>How many lines were skipped as bad.</summary>
    public int BadLines
    {
        get { lock (gate) { return session.BadLines; } }
    }

    /// <summary>Why the reading ended.</summary>
    public WatchEnd End
    {
        get { lock (gate) { return session.End; } }
    }

    /// <summary>Starts the program and the reading. Never throws: a program that cannot be started gives the end reason Failed.</summary>
    public void Start()
    {
        process = TryStart();
        if (process == null) { EndWith(WatchEnd.Failed); return; }
        reading = Task.Run(RunAsync);
    }

    /// <summary>Waits for the header.</summary>
    /// <param name="timeout">The longest wait.</param>
    /// <returns>The header, or null when none arrived in time or the reading ended before it.</returns>
    public async Task<WatchHeader?> WaitForHeaderAsync(TimeSpan timeout)
    {
        try { return await headerReady.Task.WaitAsync(timeout); }
        catch (TimeoutException) { return null; }
    }

    /// <summary>Gives the latest line when it is fresh. A line that is older than three periods and a second is stale, because the program stalled or ended.</summary>
    /// <param name="nowSeconds">The time now on the clock of the calibrator (ProcessClock).</param>
    /// <returns>The line, or null.</returns>
    public WatchSample? Latest(double nowSeconds)
    {
        lock (gate)
        {
            return stopped || session.Header == null ? null : session.Fresh(nowSeconds, StalePeriods * session.Header.PeriodMs / 1000.0 + StaleExtraSeconds);
        }
    }

    /// <summary>Closes the input of the program, waits for it to end and for the reading to finish. Never throws. It may be called many times.</summary>
    /// <returns>A task that ends when everything is closed.</returns>
    public Task StopAsync()
    {
        stopping ??= StopCoreAsync();
        return stopping;
    }

    /// <summary>Starts the program.</summary>
    /// <returns>The program, or null when it could not be started.</returns>
    private IWatchProcess? TryStart()
    {
        try { return launcher.Start(); }
        catch (Exception) { return null; }
    }

    /// <summary>Records the end of the reading, unless it already ended, and lets a waiter for the header go.</summary>
    /// <param name="reason">The reason.</param>
    private void EndWith(WatchEnd reason)
    {
        lock (gate) { session.Finish(reason); }
        headerReady.TrySetResult(Header);
    }

    /// <summary>The reading task: reads to the end and records how it ended.</summary>
    private async Task RunAsync()
    {
        try { await ReadAllAsync(); }
        catch (Exception) { EndWith(WatchEnd.Failed); return; }
        EndWith(WatchEnd.InputEnded);
    }

    /// <summary>Reads lines until the output ends or the session says stop.</summary>
    private async Task ReadAllAsync()
    {
        BoundedLineReader reader = new(process!.Output, WatchParser.MaxLineBytes);
        while (await ReadOneAsync(reader)) { }
    }

    /// <summary>Reads one line and gives it to the session.</summary>
    /// <param name="reader">The line reader.</param>
    /// <returns>True to go on reading.</returns>
    private async Task<bool> ReadOneAsync(BoundedLineReader reader)
    {
        WatchLine? line = await reader.ReadAsync(cancel.Token);
        if (line == null) { return false; }
        bool goOn = Give(line.Value);
        headerReady.TrySetResult(Header);
        return goOn;
    }

    /// <summary>Gives a line to the session.</summary>
    /// <param name="line">The line.</param>
    /// <returns>True to go on reading.</returns>
    private bool Give(WatchLine line)
    {
        lock (gate) { return session.Accept(line, ProcessClock.Seconds); }
    }

    /// <summary>Does the work of StopAsync.</summary>
    private async Task StopCoreAsync()
    {
        lock (gate) { stopped = true; }
        IWatchProcess? running = process;
        if (running != null) { await Task.Run(() => CloseAndWait(running)); }
        cancel.Cancel();
        await WaitQuietlyAsync(reading, TimeSpan.FromSeconds(ReadingWaitSeconds));
        EndWith(WatchEnd.Stopped);
    }

    /// <summary>Closes the input of the program, waits for it to end, kills it only when it does not, and releases it.</summary>
    /// <param name="running">The program.</param>
    private static void CloseAndWait(IWatchProcess running)
    {
        running.CloseInput();
        if (!running.WaitForExit(ExitWaitMs)) { KillAndWait(running); }
        running.Dispose();
    }

    /// <summary>Ends a program that did not end after its input was closed.</summary>
    /// <param name="running">The program.</param>
    private static void KillAndWait(IWatchProcess running)
    {
        running.Kill();
        running.WaitForExit(KillWaitMs);
    }

    /// <summary>Waits for a task for a limited time and ignores a time-out and any error of the task.</summary>
    /// <param name="task">The task.</param>
    /// <param name="limit">The longest wait.</param>
    private static async Task WaitQuietlyAsync(Task task, TimeSpan limit)
    {
        try { await task.WaitAsync(limit); }
        catch (Exception) { }
    }
}

/// <summary>A real program with redirected input and output.</summary>
/// <param name="process">The started process.</param>
internal sealed class ScanProcess(Process process) : IWatchProcess
{
    /// <summary>Its standard output as UTF-8 text.</summary>
    public TextReader Output => process.StandardOutput;

    /// <summary>Closes its standard input. A scanner in watch mode ends when it sees the end of its input.</summary>
    public void CloseInput()
    {
        try { process.StandardInput.Close(); }
        catch (IOException) { }
    }

    /// <summary>Waits for it to end.</summary>
    /// <param name="milliseconds">The longest wait.</param>
    /// <returns>True when it ended.</returns>
    public bool WaitForExit(int milliseconds) => process.WaitForExit(milliseconds);

    /// <summary>Ends it and its children. A program that has already ended is not an error.</summary>
    public void Kill()
    {
        try { process.Kill(true); }
        catch (InvalidOperationException) { }
        catch (Win32Exception) { }
    }

    /// <summary>Releases the process object.</summary>
    public void Dispose() => process.Dispose();
}

/// <summary>Starts a real program: no shell, no window, standard input and output redirected, standard error read and thrown away.</summary>
/// <param name="path">Full path of the program.</param>
/// <param name="arguments">The fixed arguments, one entry each (no shell, so no quoting).</param>
internal sealed class ScanProcessLauncher(string path, IReadOnlyList<string> arguments) : IWatchLauncher
{
    /// <summary>Starts the program.</summary>
    /// <returns>The running program.</returns>
    /// <exception cref="IOException">It cannot be started (missing file, no permission).</exception>
    public IWatchProcess Start()
    {
        try { return StartProcess(); }
        catch (Win32Exception ex) { throw new IOException("the program could not be started", ex); }
        catch (InvalidOperationException ex) { throw new IOException("the program could not be started", ex); }
    }

    /// <summary>Starts the process.</summary>
    /// <returns>The running program.</returns>
    private ScanProcess StartProcess()
    {
        ProcessStartInfo info = new(path)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = new UTF8Encoding(false),
            WorkingDirectory = Path.GetDirectoryName(path) ?? ""
        };
        foreach (string argument in arguments) { info.ArgumentList.Add(argument); }
        Process process = Process.Start(info) ?? throw new IOException("the program did not start");
        process.BeginErrorReadLine();
        return new ScanProcess(process);
    }
}
