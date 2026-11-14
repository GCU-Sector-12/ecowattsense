// ServicePipe.cs: the client side of the session pipe (SPEC section 6).
// Who uses it: ServicePipeReporter (every 60 seconds and on unlock) and Test-ServicePipe.ps1.
// What it does: builds the one-line message "EWS1 idle=<seconds> bright=<0-100 or ->" and writes it to
// \\.\pipe\EcoWattSense.Service.Session. The service is the pipe server. A missing pipe, a busy pipe or a failed write
// is counted and ignored: the client never crashes and never shows a dialog because of it.
#nullable enable
using System;
using System.Globalization;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Builds the text of the pipe message.</summary>
public static class ServicePipeMessage
{
    /// <summary>The pipe name without the \\.\pipe\ prefix.</summary>
    public const string PipeName = "EcoWattSense.Service.Session";

    /// <summary>
    /// The largest idle value sent: 4294967 seconds is the 32-bit millisecond tick period of GetLastInputInfo
    /// (about 49.7 days), so a larger value cannot come from a real reading.
    /// </summary>
    public const double MaxIdleSeconds = 4294967;

    /// <summary>Builds the message.</summary>
    /// <param name="idleSeconds">Seconds since the last keyboard or mouse input.</param>
    /// <param name="brightness">The screen brightness in percent, or null when it could not be read.</param>
    /// <returns>
    /// The message with a final line feed, at most 128 ASCII bytes. Null when the idle value is not a finite number
    /// or is negative, because the service would drop such a message. An idle value above the maximum is capped;
    /// a brightness that is missing or outside 0 to 100 is sent as "-".
    /// </returns>
    public static string? Format(double idleSeconds, int? brightness)
    {
        if (!double.IsFinite(idleSeconds) || idleSeconds < 0) return null;
        long idle = (long)Math.Min(Math.Floor(idleSeconds), MaxIdleSeconds);
        return $"EWS1 idle={idle.ToString(CultureInfo.InvariantCulture)} bright={BrightnessText(brightness)}\n";
    }

    /// <summary>Gives the brightness field of the message.</summary>
    /// <param name="brightness">The brightness in percent, or null.</param>
    /// <returns>The number for 0 to 100, otherwise "-".</returns>
    private static string BrightnessText(int? brightness)
        => brightness is >= 0 and <= 100 ? brightness.Value.ToString(CultureInfo.InvariantCulture) : "-";
}

/// <summary>Writes one message to the service pipe and counts successes and failures.</summary>
public sealed class ServicePipeSender
{
    private readonly string pipeName;
    private readonly int connectTimeoutMilliseconds;
    private long sent;
    private long failed;

    /// <summary>Creates a sender.</summary>
    /// <param name="pipeName">The pipe name; tests use their own name.</param>
    /// <param name="connectTimeoutMilliseconds">How long to wait for a busy or missing pipe before the send is counted as failed.</param>
    public ServicePipeSender(string pipeName = ServicePipeMessage.PipeName, int connectTimeoutMilliseconds = 500)
    {
        this.pipeName = pipeName;
        this.connectTimeoutMilliseconds = connectTimeoutMilliseconds;
    }

    /// <summary>The number of messages written.</summary>
    public long Sent => Interlocked.Read(ref sent);

    /// <summary>The number of messages that could not be written (including messages that were not built).</summary>
    public long Failed => Interlocked.Read(ref failed);

    /// <summary>Writes one message.</summary>
    /// <param name="message">The message from <see cref="ServicePipeMessage.Format"/>, or null when none could be built.</param>
    /// <returns>True when the message was written. False when it was null or the pipe was missing, busy or refused the write.</returns>
    public bool Send(string? message)
    {
        if (message is null) return Fail();
        try
        {
            Write(message);
            Interlocked.Increment(ref sent);
            return true;
        }
        catch (Exception exception) when (exception is IOException or TimeoutException or UnauthorizedAccessException or OperationCanceledException)
        {
            return Fail();
        }
    }

    /// <summary>Opens the pipe, writes the message bytes and closes the pipe.</summary>
    /// <param name="message">The message text (ASCII).</param>
    /// <exception cref="TimeoutException">The pipe does not exist or stays busy for the whole time limit.</exception>
    /// <exception cref="OperationCanceledException">The service does not read the message within the time limit.</exception>
    /// <exception cref="IOException">The write fails.</exception>
    /// <exception cref="UnauthorizedAccessException">The pipe refuses this user.</exception>
    private void Write(string message)
    {
        using var pipe = new NamedPipeClientStream(".", pipeName, PipeDirection.Out, PipeOptions.Asynchronous);
        pipe.Connect(connectTimeoutMilliseconds);
        byte[] bytes = Encoding.ASCII.GetBytes(message);
        // The pipe has no buffer on a plain server, so a write waits until the service reads it. The limit keeps a hung service from holding the sender.
        using var limit = new CancellationTokenSource(connectTimeoutMilliseconds);
        pipe.WriteAsync(bytes, 0, bytes.Length, limit.Token).GetAwaiter().GetResult();
    }

    /// <summary>Counts one failed send.</summary>
    /// <returns>Always false, so callers can return it.</returns>
    private bool Fail()
    {
        Interlocked.Increment(ref failed);
        return false;
    }
}
