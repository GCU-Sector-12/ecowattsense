// BoundedLineReader.cs: reads the lines of a stream and never holds more than one limited line in memory.
// Who uses it: DiagFileReader (diag-*.jsonl) and LogTailReader (service.log).
// Why it exists: StreamReader.ReadLine has no limit, and a hostile file could be one line of 60 MB. This reader stops
// storing at the limit, skips the rest of that line and says "too long". It also says whether a line ended with a line
// feed: a last line without one may still be written by the service. A carriage return before the line feed is dropped.
#nullable enable
using System.IO;
using System.Text;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>The kind of result of one read.</summary>
public enum LineKind
{
    /// <summary>A line was read.</summary>
    Line,
    /// <summary>A line was longer than the limit. Its text is not kept.</summary>
    TooLong,
    /// <summary>The end of the stream.</summary>
    End
}

/// <summary>One result of <see cref="BoundedLineReader.Next"/>.</summary>
/// <param name="Kind">What was read.</param>
/// <param name="Text">The UTF-8 text of the line, without the line end. Empty for TooLong and End.</param>
/// <param name="Terminated">True when the line ended with a line feed.</param>
public readonly record struct LineRead(LineKind Kind, string Text, bool Terminated);

/// <summary>Reads lines of at most a given number of bytes.</summary>
public sealed class BoundedLineReader
{
    private readonly Stream stream;
    private readonly int maxBytes;
    private readonly byte[] buffer = new byte[16 * 1024];
    private readonly byte[] line;
    private int position;
    private int length;
    private int lineLength;
    private bool tooLong;

    /// <summary>Creates a reader.</summary>
    /// <param name="stream">The stream to read. The reader does not close it.</param>
    /// <param name="maxBytes">The most bytes in one line, without the line end.</param>
    public BoundedLineReader(Stream stream, int maxBytes)
    {
        this.stream = stream;
        this.maxBytes = maxBytes;
        line = new byte[maxBytes];
    }

    /// <summary>Reads the next line.</summary>
    /// <returns>The line, or a TooLong or End result.</returns>
    /// <exception cref="IOException">The stream cannot be read.</exception>
    public LineRead Next()
    {
        lineLength = 0;
        tooLong = false;
        int last;
        while ((last = NextByte()) >= 0 && last != '\n') Append(last);
        bool terminated = last == '\n';
        if (last < 0 && lineLength == 0 && !tooLong) return new LineRead(LineKind.End, "", false);
        if (tooLong) return new LineRead(LineKind.TooLong, "", terminated);
        return new LineRead(LineKind.Line, Decode(), terminated);
    }

    /// <summary>Gives the next byte of the stream.</summary>
    /// <returns>The byte, or -1 at the end.</returns>
    private int NextByte()
    {
        if (position >= length && !Fill()) return -1;
        return buffer[position++];
    }

    /// <summary>Reads the next block of the stream into the buffer.</summary>
    /// <returns>False at the end of the stream.</returns>
    private bool Fill()
    {
        length = stream.Read(buffer, 0, buffer.Length);
        position = 0;
        return length > 0;
    }

    /// <summary>Stores one byte of the line, or marks the line as too long.</summary>
    /// <param name="value">The byte.</param>
    private void Append(int value)
    {
        if (lineLength >= maxBytes)
        {
            tooLong = true;
            return;
        }
        line[lineLength++] = (byte)value;
    }

    /// <summary>Decodes the stored bytes as UTF-8, without a closing carriage return.</summary>
    /// <returns>The text. Bytes that are not UTF-8 become the replacement character.</returns>
    private string Decode()
    {
        int count = lineLength > 0 && line[lineLength - 1] == '\r' ? lineLength - 1 : lineLength;
        return Encoding.UTF8.GetString(line, 0, count);
    }
}
