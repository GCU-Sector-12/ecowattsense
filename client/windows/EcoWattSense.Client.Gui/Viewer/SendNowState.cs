// SendNowState.cs: the text and the button state of the "Send now" part of the Last report card (SPEC section 9).
// Who uses it: the viewer window (MainWindow.SendNow.cs) and Test-SendNow.ps1.
// It is pure: the facts come in, the text and the state go out. The pipe write and the 30 second lock live in the window.
#nullable enable
using System;
using System.Globalization;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Chooses the text and the button state for sending the waiting reports at once.</summary>
public static class SendNowState
{
    /// <summary>The exact pipe message for "send now", with its final line feed.</summary>
    public const string Message = "EWS1 send\n";

    /// <summary>How long the button stays disabled after a click. The service accepts one request per 30 seconds.</summary>
    public const int LockSeconds = 30;

    /// <summary>The line shown while the request is out and status.json has not changed yet.</summary>
    public const string SendingLine = "Sending...";

    /// <summary>The line shown when the pipe write failed.</summary>
    public const string FailedLine = "The service did not answer.";

    /// <summary>Gives the line about the waiting reports.</summary>
    /// <param name="queued">The number of waiting reports from status.json, or null when the field is missing.</param>
    /// <returns>An empty text for null, "All reports sent." for 0, otherwise "Waiting to send: N reports".</returns>
    public static string QueueLine(int? queued)
    {
        if (queued is null) return "";
        if (queued == 0) return "All reports sent.";
        return $"Waiting to send: {queued.Value.ToString(CultureInfo.InvariantCulture)} reports";
    }

    /// <summary>Gives the line under the report time.</summary>
    /// <param name="queued">The number of waiting reports, or null.</param>
    /// <param name="sending">True after a click until status.json changes.</param>
    /// <param name="failed">True when the last pipe write failed and status.json has not changed since.</param>
    /// <returns>The failure line first, then the sending line, then the queue line.</returns>
    public static string Note(int? queued, bool sending, bool failed)
    {
        if (failed) return FailedLine;
        if (sending) return SendingLine;
        return QueueLine(queued);
    }

    /// <summary>Tells whether the button may be clicked.</summary>
    /// <param name="queued">The number of waiting reports, or null.</param>
    /// <param name="serverSet">True when status.json says a server address and key are set.</param>
    /// <param name="nowUtc">The current time.</param>
    /// <param name="lockedUntilUtc">The end of the lock after the last click; <see cref="DateTime.MinValue"/> when there was none.</param>
    /// <returns>True when reports wait, a server is set and the lock is over.</returns>
    public static bool ButtonEnabled(int? queued, bool serverSet, DateTime nowUtc, DateTime lockedUntilUtc)
    {
        if (queued is not > 0) return false;
        if (!serverSet) return false;
        return nowUtc >= lockedUntilUtc;
    }
}
