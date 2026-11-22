// FakeWatch.cs: a fake program for the stream, so the client can be checked without ews-scan.exe and without Windows.
// Who uses it: ClientChecks. The fake prints the lines it is given. With a hold it then waits (as a running scanner does) until its input is closed.
using EnergyCalibrator;

namespace CalibratorTests;

/// <summary>A text reader that gives lines, then either ends or waits until it is released.</summary>
internal sealed class WaitingReader : TextReader
{
    private readonly Queue<string> lines;
    private readonly bool holdAtEnd;
    private readonly ManualResetEventSlim release = new(false);

    /// <summary>Creates the reader.</summary>
    /// <param name="lines">The lines to give.</param>
    /// <param name="holdAtEnd">True to wait for Release after the last line instead of ending.</param>
    public WaitingReader(IEnumerable<string> lines, bool holdAtEnd)
    {
        this.lines = new Queue<string>(lines);
        this.holdAtEnd = holdAtEnd;
    }

    /// <summary>Lets a waiting reader end.</summary>
    public void Release() => release.Set();

    /// <summary>Gives the next character of the text.</summary>
    /// <returns>The character, or -1 at the end.</returns>
    public override int Read()
    {
        Span<char> one = stackalloc char[1];
        return Read(one) == 0 ? -1 : one[0];
    }

    /// <summary>Gives the next character without taking it.</summary>
    /// <returns>The character, or -1 at the end.</returns>
    public override int Peek()
    {
        if (pending.Length == 0 && !Fill()) { return -1; }
        return pending[0];
    }

    /// <summary>Gives the characters that are available, as a real pipe does: it waits only when there is nothing.</summary>
    /// <param name="buffer">Where to put them.</param>
    /// <returns>The number of characters; 0 at the end.</returns>
    public override int Read(Span<char> buffer)
    {
        if (pending.Length == 0 && !Fill()) { return 0; }
        int count = Math.Min(buffer.Length, pending.Length);
        pending.AsSpan(0, count).CopyTo(buffer);
        pending = pending[count..];
        return count;
    }

    /// <summary>Gives the characters that are available.</summary>
    /// <param name="buffer">The array.</param>
    /// <param name="index">First position.</param>
    /// <param name="count">Most characters.</param>
    /// <returns>The number of characters; 0 at the end.</returns>
    public override int Read(char[] buffer, int index, int count) => Read(buffer.AsSpan(index, count));

    /// <summary>Reads on a pool thread.</summary>
    /// <param name="buffer">Where to put the characters.</param>
    /// <param name="cancellationToken">Not used: a pipe read cannot be cancelled either.</param>
    /// <returns>The number of characters; 0 at the end.</returns>
    public override ValueTask<int> ReadAsync(Memory<char> buffer, CancellationToken cancellationToken = default)
    {
        return new ValueTask<int>(Task.Run(() => Read(buffer.Span)));
    }

    private string pending = "";

    /// <summary>Takes the next line into the pending text, or waits when it is held.</summary>
    /// <returns>False at the end.</returns>
    private bool Fill()
    {
        if (lines.Count > 0) { pending = lines.Dequeue() + "\n"; return true; }
        if (!holdAtEnd) { return false; }
        release.Wait();
        return false;
    }
}

/// <summary>A fake running program.</summary>
internal sealed class FakeProcess : IWatchProcess
{
    private readonly WaitingReader reader;

    /// <summary>Creates the fake.</summary>
    /// <param name="lines">The lines it prints.</param>
    /// <param name="holdAtEnd">True when it keeps running until its input is closed.</param>
    public FakeProcess(IEnumerable<string> lines, bool holdAtEnd)
    {
        reader = new WaitingReader(lines, holdAtEnd);
    }

    /// <summary>True after CloseInput was called.</summary>
    public bool InputClosed { get; private set; }

    /// <summary>True after Kill was called.</summary>
    public bool Killed { get; private set; }

    /// <summary>True after Dispose was called.</summary>
    public bool Disposed { get; private set; }

    /// <summary>Its output.</summary>
    public TextReader Output => reader;

    /// <summary>Closes the input; a waiting fake then ends, as the scanner does.</summary>
    public void CloseInput()
    {
        InputClosed = true;
        reader.Release();
    }

    /// <summary>Waits for it to end. The fake ends as soon as its output is released.</summary>
    /// <param name="milliseconds">Not used.</param>
    /// <returns>True when the input was closed.</returns>
    public bool WaitForExit(int milliseconds) => InputClosed;

    /// <summary>Ends it at once.</summary>
    public void Kill()
    {
        Killed = true;
        reader.Release();
    }

    /// <summary>Marks it disposed.</summary>
    public void Dispose() => Disposed = true;
}

/// <summary>Starts a fake program, or fails as a missing file does.</summary>
internal sealed class FakeLauncher : IWatchLauncher
{
    private readonly FakeProcess? process;

    /// <summary>Creates a launcher that gives a fake program.</summary>
    /// <param name="process">The fake, or null to fail like a missing file.</param>
    public FakeLauncher(FakeProcess? process)
    {
        this.process = process;
    }

    /// <summary>How many times Start was called.</summary>
    public int Starts { get; private set; }

    /// <summary>Starts the program.</summary>
    /// <returns>The fake.</returns>
    /// <exception cref="IOException">There is no program.</exception>
    public IWatchProcess Start()
    {
        Starts++;
        return process ?? throw new FileNotFoundException("no program");
    }
}
