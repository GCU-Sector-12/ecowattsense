// WatchLines.cs: reads lines from a text reader with a hard size limit.
// Who uses it: WatchClient. The other program could send an endless line; this reader never keeps more than the limit.
using System.IO;
using System.Text;

namespace EnergyCalibrator;

/// <summary>One line read from the stream.</summary>
/// <param name="Text">The line without its line break; null when the line was longer than the limit (its rest was thrown away).</param>
/// <param name="TooLong">True when the line was longer than the limit.</param>
internal readonly record struct WatchLine(string? Text, bool TooLong);

/// <summary>Reads lines of at most a given size. It keeps at most one buffer and one line, whatever the other program sends.</summary>
/// <param name="reader">Where the text comes from.</param>
/// <param name="maxBytes">Largest line in UTF-8 bytes.</param>
internal sealed class BoundedLineReader(TextReader reader, int maxBytes)
{
    private const int BufferChars = 4096;
    private readonly char[] buffer = new char[BufferChars];
    private readonly StringBuilder line = new();
    private int start;
    private int end;
    private bool tooLong;
    private bool finished;

    /// <summary>Reads the next line.</summary>
    /// <param name="cancel">Cancels the wait where the reader allows it.</param>
    /// <returns>The line, or null when the text has ended.</returns>
    public async Task<WatchLine?> ReadAsync(CancellationToken cancel)
    {
        WatchLine? ready = TakeLine();
        while (ready == null && !finished)
        {
            await FillAsync(cancel);
            ready = TakeLine();
        }
        return ready ?? LastLine();
    }

    /// <summary>Reads more text into the buffer. The end of the text sets "finished".</summary>
    /// <param name="cancel">Cancels the wait where the reader allows it.</param>
    private async Task FillAsync(CancellationToken cancel)
    {
        int count = await reader.ReadAsync(buffer.AsMemory(), cancel);
        start = 0;
        end = count;
        finished = count == 0;
    }

    /// <summary>Takes the characters of the buffer up to the next line break.</summary>
    /// <returns>The line when a line break was found; null when the buffer holds only the start of a line.</returns>
    private WatchLine? TakeLine()
    {
        int newline = Array.IndexOf(buffer, '\n', start, end - start);
        int stop = newline < 0 ? end : newline;
        Append(start, stop);
        start = newline < 0 ? end : newline + 1;
        return newline < 0 ? null : Complete();
    }

    /// <summary>Gives the last line when the text ended without a line break.</summary>
    /// <returns>The line, or null when nothing is left.</returns>
    private WatchLine? LastLine()
    {
        bool pending = line.Length > 0 || tooLong;
        return pending ? Complete() : null;
    }

    /// <summary>Adds characters to the line, or throws them away once the line is too long.</summary>
    /// <param name="from">First character.</param>
    /// <param name="to">End of the characters (not included).</param>
    private void Append(int from, int to)
    {
        if (tooLong) { return; }
        line.Append(buffer, from, to - from);
        if (line.Length <= maxBytes) { return; }
        tooLong = true;
        line.Clear();
    }

    /// <summary>Finishes the line that was collected and starts a new one.</summary>
    /// <returns>The line; its text is null when it was too long.</returns>
    private WatchLine Complete()
    {
        string text = line.ToString().TrimEnd('\r');
        bool over = tooLong || Encoding.UTF8.GetByteCount(text) > maxBytes;
        line.Clear();
        tooLong = false;
        return over ? new WatchLine(null, true) : new WatchLine(text, false);
    }
}
