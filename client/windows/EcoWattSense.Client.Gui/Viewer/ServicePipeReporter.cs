// ServicePipeReporter.cs: sends the pipe message of SPEC section 6 once at start, every 60 seconds and on each session unlock.
// Who uses it: the viewer window (MainWindow.Viewer.cs). Only the viewer mode starts it.
// Why a timer: SPEC 6 allows exactly one 60 second timer for this message; everything else in the viewer is event driven.
// The send runs on a thread-pool thread so a slow WMI query or a busy pipe never blocks the window.
#nullable enable
using System;
using System.Threading;
using Microsoft.Win32;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Sends the idle time and brightness to the service pipe.</summary>
public sealed class ServicePipeReporter : IDisposable
{
    private static readonly TimeSpan Period = TimeSpan.FromSeconds(60);
    private readonly ServicePipeSender sender;
    private readonly Func<double?> readIdleSeconds;
    private readonly Func<int?> readBrightness;
    private readonly object gate = new();
    private Timer? timer;
    private bool started;

    /// <summary>Creates a reporter.</summary>
    /// <param name="sender">Writes the message to the pipe.</param>
    /// <param name="readIdleSeconds">Gives the idle seconds, or null when unknown.</param>
    /// <param name="readBrightness">Gives the brightness in percent, or null when unknown.</param>
    public ServicePipeReporter(ServicePipeSender sender, Func<double?> readIdleSeconds, Func<int?> readBrightness)
    {
        this.sender = sender;
        this.readIdleSeconds = readIdleSeconds;
        this.readBrightness = readBrightness;
    }

    /// <summary>Starts the 60 second timer (first send at once) and listens for session unlock.</summary>
    public void Start()
    {
        if (started) return;
        started = true;
        SystemEvents.SessionSwitch += OnSessionSwitch;
        timer = new Timer(OnTimer, null, TimeSpan.Zero, Period);
    }

    /// <summary>Stops the timer and the unlock listener.</summary>
    public void Dispose()
    {
        SystemEvents.SessionSwitch -= OnSessionSwitch;
        timer?.Dispose();
        timer = null;
    }

    /// <summary>Timer callback: sends one message.</summary>
    /// <param name="state">Not used.</param>
    private void OnTimer(object? state) => SendNow();

    /// <summary>Sends one message when the session was just unlocked.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">The session change; only SessionUnlock sends.</param>
    private void OnSessionSwitch(object sender, SessionSwitchEventArgs e)
    {
        if (e.Reason == SessionSwitchReason.SessionUnlock) SendNow();
    }

    /// <summary>Builds and sends one message. A send that overlaps another one is skipped, not queued.</summary>
    private void SendNow()
    {
        if (!Monitor.TryEnter(gate)) return;
        try { sender.Send(BuildMessage()); }
        finally { Monitor.Exit(gate); }
    }

    /// <summary>Builds the message from the current readings.</summary>
    /// <returns>The message, or null when the idle time is unknown (the sender counts that as a failed send).</returns>
    private string? BuildMessage()
    {
        double? idle = readIdleSeconds();
        return idle is null ? null : ServicePipeMessage.Format(idle.Value, readBrightness());
    }
}
