// Handles activation requests from duplicate Windows GUI launches.

using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Win32.SafeHandles;

namespace EcoWattSense.Client.Gui;

internal sealed class GuiActivationPipe : IAsyncDisposable
{
    private static readonly TimeSpan Timeout = TimeSpan.FromSeconds(3);
    private static readonly byte[] Open = "OPEN"u8.ToArray();
    private static readonly byte[] Opened = "OPENED"u8.ToArray();
    private const PipeOptions Options = PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly;
    private readonly CancellationTokenSource stop = new();
    private readonly Task listener;

    /// <summary>Starts the listener on a pool thread when called from one, keeping pipe I/O off the dispatcher.</summary>
    /// <param name="lockPath">The path to the instance lock.</param>
    /// <param name="restore">Restores the running window and reports whether it opened.</param>
    /// <exception cref="IOException">The server pipe could not be created.</exception>
    internal GuiActivationPipe(string lockPath, Func<CancellationToken, Task<bool>> restore)
    {
        var pipe = new NamedPipeServerStream(NameFor(lockPath), PipeDirection.InOut, 1,
            PipeTransmissionMode.Message, Options);
        listener = ListenAsync(pipe, restore);
    }

    /// <summary>Builds the pipe name for the current user, session and lock.</summary>
    /// <param name="lockPath">The path to the instance lock.</param>
    /// <returns>The activation pipe name.</returns>
    internal static string NameFor(string lockPath)
    {
        using var identity = WindowsIdentity.GetCurrent();
        using var process = Process.GetCurrentProcess();
        var path = Path.GetFullPath(lockPath).ToUpperInvariant();
        var hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(path)));
        return $"EcoWattSense.Open.{identity.User!.Value}.{process.SessionId}.{hash}";
    }

    /// <summary>Listens for activation requests until shutdown.</summary>
    /// <param name="pipe">The server pipe, which this method disposes.</param>
    /// <param name="restore">Restores the running window and reports whether it opened.</param>
    /// <returns>A task that completes when the listener stops.</returns>
    private async Task ListenAsync(NamedPipeServerStream pipe, Func<CancellationToken, Task<bool>> restore)
    {
        using var server = pipe;
        while (!stop.IsCancellationRequested)
            await ListenOnceAsync(server, restore);
    }

    /// <summary>Handles one connection attempt and resets the pipe.</summary>
    /// <param name="pipe">The server pipe.</param>
    /// <param name="restore">Restores the running window and reports whether it opened.</param>
    /// <returns>A task that completes after the pipe is reset.</returns>
    private async Task ListenOnceAsync(NamedPipeServerStream pipe, Func<CancellationToken, Task<bool>> restore)
    {
        try
        {
            await pipe.WaitForConnectionAsync(stop.Token);
            await HandleConnectionAsync(pipe, restore);
        }
        catch (OperationCanceledException) { /* Shutdown or a stalled client. */ }
        catch (IOException) { /* The launching process may exit at any point. */ }
        finally
        {
            Disconnect(pipe);
        }
    }

    /// <summary>Restores the window for a valid request from the current session.</summary>
    /// <param name="pipe">The server pipe.</param>
    /// <param name="restore">Restores the running window and reports whether it opened.</param>
    /// <returns>A task that completes when the request has been handled.</returns>
    /// <exception cref="IOException">The connection fails.</exception>
    /// <exception cref="OperationCanceledException">The request times out or the listener stops.</exception>
    private async Task HandleConnectionAsync(NamedPipeServerStream pipe, Func<CancellationToken, Task<bool>> restore)
    {
        using var deadline = CancellationTokenSource.CreateLinkedTokenSource(stop.Token);
        deadline.CancelAfter(Timeout);
        using var process = Process.GetCurrentProcess();
        if (!GetNamedPipeClientSessionId(pipe.SafePipeHandle, out uint session) ||
            session != process.SessionId || !await ReceiveAsync(pipe, Open, deadline.Token) ||
            !await restore(deadline.Token))
            return;

        await pipe.WriteAsync(Opened, deadline.Token);
        // Disconnect discards unread data. Let the client read its reply and close first.
        _ = await pipe.ReadAsync(new byte[1], deadline.Token);
    }

    /// <summary>Resets the server pipe even after a broken connection.</summary>
    /// <param name="pipe">The server pipe.</param>
    private static void Disconnect(NamedPipeServerStream pipe)
    {
        // IsConnected can already be false after a broken pipe.
        try { pipe.Disconnect(); } catch (InvalidOperationException) { }
    }

    /// <summary>Asks the running GUI to open its window.</summary>
    /// <param name="lockPath">The path to the instance lock.</param>
    /// <param name="cancellation">Cancels the request.</param>
    /// <returns>True if the running GUI confirms that it opened its window.</returns>
    internal static async Task<bool> RequestAsync(string lockPath, CancellationToken cancellation)
    {
        using var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellation);
        deadline.CancelAfter(Timeout);
        try
        {
            using var pipe = new NamedPipeClientStream(".", NameFor(lockPath), PipeDirection.InOut, Options,
                TokenImpersonationLevel.Identification);
            await pipe.ConnectAsync(deadline.Token);
            pipe.ReadMode = PipeTransmissionMode.Message;
            using var process = Process.GetCurrentProcess();
            if (!GetNamedPipeServerSessionId(pipe.SafePipeHandle, out uint session) || session != process.SessionId)
                return false;
            if (GetNamedPipeServerProcessId(pipe.SafePipeHandle, out uint pid))
                AllowSetForegroundWindow(pid); // Windows can decline focus even when restore succeeds.
            await pipe.WriteAsync(Open, deadline.Token);
            return await ReceiveAsync(pipe, Opened, deadline.Token);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or OperationCanceledException or TimeoutException)
        {
            return false;
        }
    }

    /// <summary>Reads and checks a single pipe message.</summary>
    /// <param name="pipe">The connected pipe.</param>
    /// <param name="expected">The expected message bytes.</param>
    /// <param name="token">Cancels the read.</param>
    /// <returns>True if the complete message matches.</returns>
    /// <exception cref="IOException">The read fails.</exception>
    /// <exception cref="OperationCanceledException">The read is cancelled.</exception>
    private static async Task<bool> ReceiveAsync(PipeStream pipe, byte[] expected, CancellationToken token)
    {
        var buffer = new byte[expected.Length];
        int count = await pipe.ReadAsync(buffer, token);
        return count == expected.Length && pipe.IsMessageComplete && buffer.AsSpan().SequenceEqual(expected);
    }

    /// <summary>Stops the listener and disposes its cancellation source.</summary>
    /// <returns>A task that completes when the listener has stopped.</returns>
    public async ValueTask DisposeAsync()
    {
        stop.Cancel();
        try { await listener.ConfigureAwait(false); }
        finally { stop.Dispose(); }
    }

    [DllImport("kernel32.dll")]
    private static extern bool GetNamedPipeClientSessionId(SafePipeHandle pipe, out uint session);
    [DllImport("kernel32.dll")]
    private static extern bool GetNamedPipeServerSessionId(SafePipeHandle pipe, out uint session);
    [DllImport("kernel32.dll")]
    private static extern bool GetNamedPipeServerProcessId(SafePipeHandle pipe, out uint pid);
    [DllImport("user32.dll")]
    private static extern bool AllowSetForegroundWindow(uint pid);
}
