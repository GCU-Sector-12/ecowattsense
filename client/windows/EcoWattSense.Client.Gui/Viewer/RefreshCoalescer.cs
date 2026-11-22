// RefreshCoalescer.cs: turns a burst of file events into one queued refresh (SPEC 9).
// Who uses it: the viewer window (MainWindow.Viewer.cs) for the history files. Appending one row to a history file
// raises several change events at once; the window must read the history once, not once per event.
// How: the first Request queues one run through the post delegate (the dispatcher in the app, a plain queue in tests).
// Requests that arrive before that run starts are dropped. There is no timer; the dispatcher does the waiting.
#nullable enable
using System;
using System.Threading;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Collapses many requests into one queued run of a work action.</summary>
public sealed class RefreshCoalescer
{
    private readonly Action<Action> post;
    private readonly Action work;
    private int pending;

    /// <summary>Creates the coalescer.</summary>
    /// <param name="post">Queues an action to run later, for example Dispatcher.BeginInvoke at background priority.</param>
    /// <param name="work">The refresh to run once per burst of requests.</param>
    public RefreshCoalescer(Action<Action> post, Action work)
    {
        this.post = post;
        this.work = work;
    }

    /// <summary>Asks for a refresh. Safe to call from any thread.</summary>
    public void Request()
    {
        if (Interlocked.Exchange(ref pending, 1) == 1) return;
        post(Run);
    }

    /// <summary>Clears the flag before the work, so a request that arrives during the work queues the next run.</summary>
    private void Run()
    {
        Interlocked.Exchange(ref pending, 0);
        work();
    }
}
