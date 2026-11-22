// WatchSession.cs: the state of one stream: the header, the latest good line, the count of bad lines and why the reading ended.
// Who uses it: WatchClient feeds it line by line. It has no thread, no process and no clock of its own: the caller passes the time.
namespace EnergyCalibrator;

/// <summary>Why the reading of the stream ended.</summary>
internal enum WatchEnd
{
    /// <summary>The stream is still being read.</summary>
    None,
    /// <summary>The other program closed its output.</summary>
    InputEnded,
    /// <summary>The first line was not a good header.</summary>
    BadHeader,
    /// <summary>A header came after the first line.</summary>
    SecondHeader,
    /// <summary>The other program could not be started or read.</summary>
    Failed,
    /// <summary>The calibrator stopped the reading.</summary>
    Stopped
}

/// <summary>Keeps what is known about one stream. It is not thread safe: WatchClient guards it with a lock.</summary>
internal sealed class WatchSession
{
    /// <summary>The header; null until a good first line arrived.</summary>
    public WatchHeader? Header { get; private set; }

    /// <summary>The latest good line; null when none arrived.</summary>
    public WatchSample? Latest { get; private set; }

    /// <summary>The time on the caller's clock when the latest good line arrived.</summary>
    public double LatestSeconds { get; private set; }

    /// <summary>How many lines were skipped as bad.</summary>
    public int BadLines { get; private set; }

    /// <summary>Why the reading ended.</summary>
    public WatchEnd End { get; private set; }

    /// <summary>Takes one line.</summary>
    /// <param name="line">The line.</param>
    /// <param name="nowSeconds">The time now on the caller's clock.</param>
    /// <returns>True to go on reading; false when the reading must end.</returns>
    public bool Accept(WatchLine line, double nowSeconds)
    {
        if (End != WatchEnd.None) { return false; }
        return Header == null ? AcceptHeader(line) : AcceptSample(line, nowSeconds);
    }

    /// <summary>Records that the reading ended for a reason, unless it already ended.</summary>
    /// <param name="reason">The reason.</param>
    public void Finish(WatchEnd reason)
    {
        if (End != WatchEnd.None) { return; }
        End = reason;
    }

    /// <summary>Gives the latest line when it is not older than a limit.</summary>
    /// <param name="nowSeconds">The time now on the caller's clock.</param>
    /// <param name="maxAgeSeconds">The oldest age that still counts as fresh.</param>
    /// <returns>The line, or null when there is none or it is stale.</returns>
    public WatchSample? Fresh(double nowSeconds, double maxAgeSeconds)
    {
        bool fresh = Latest != null && nowSeconds - LatestSeconds <= maxAgeSeconds;
        return fresh ? Latest : null;
    }

    /// <summary>Takes the first line: it must be a good header.</summary>
    /// <param name="line">The line.</param>
    /// <returns>True when the header was good.</returns>
    private bool AcceptHeader(WatchLine line)
    {
        Header = line.Text == null ? null : WatchParser.ParseHeader(line.Text, out _);
        if (Header != null) { return true; }
        Finish(WatchEnd.BadHeader);
        return false;
    }

    /// <summary>Takes a line after the header.</summary>
    /// <param name="line">The line.</param>
    /// <param name="nowSeconds">The time now on the caller's clock.</param>
    /// <returns>True to go on reading.</returns>
    private bool AcceptSample(WatchLine line, double nowSeconds)
    {
        if (line.Text == null) { return Skip(); }
        WatchSample? sample = WatchParser.ParseSample(line.Text, Header!, Latest?.TimeMs ?? -1, out string reason);
        if (reason == WatchReasons.SecondHeader) { Finish(WatchEnd.SecondHeader); return false; }
        if (sample == null) { return Skip(); }
        Latest = sample;
        LatestSeconds = nowSeconds;
        return true;
    }

    /// <summary>Counts a bad line.</summary>
    /// <returns>True: the reading goes on.</returns>
    private bool Skip()
    {
        BadLines++;
        return true;
    }
}
