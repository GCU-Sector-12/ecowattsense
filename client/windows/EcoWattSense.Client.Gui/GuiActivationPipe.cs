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

    // Construct on a pool thread so listener setup and all pipe I/O stay off the dispatcher.
    internal GuiActivationPipe(string lockPath, Func<CancellationToken, Task<bool>> restore)
    {
        var pipe = new NamedPipeServerStream(NameFor(lockPath), PipeDirection.InOut, 1,
            PipeTransmissionMode.Message, Options);
        listener = ListenAsync(pipe, restore);
    }

    internal static string NameFor(string lockPath)
    {
        using var identity = WindowsIdentity.GetCurrent();
        using var process = Process.GetCurrentProcess();
        var path = Path.GetFullPath(lockPath).ToUpperInvariant();
        var hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(path)));
        return $"EcoWattSense.Open.{identity.User!.Value}.{process.SessionId}.{hash}";
    }

    private async Task ListenAsync(NamedPipeServerStream pipe, Func<CancellationToken, Task<bool>> restore)
    {
        using (pipe)
        {
            while (!stop.IsCancellationRequested)
            {
                try
                {
                    await pipe.WaitForConnectionAsync(stop.Token);
                    using var deadline = CancellationTokenSource.CreateLinkedTokenSource(stop.Token);
                    deadline.CancelAfter(Timeout);
                    using var process = Process.GetCurrentProcess();
                    if (GetNamedPipeClientSessionId(pipe.SafePipeHandle, out uint session) &&
                        session == process.SessionId && await ReceiveAsync(pipe, Open, deadline.Token) &&
                        await restore(deadline.Token))
                    {
                        await pipe.WriteAsync(Opened, deadline.Token);
                        // Disconnect discards unread data. Let the client read its reply and close first.
                        _ = await pipe.ReadAsync(new byte[1], deadline.Token);
                    }
                }
                catch (OperationCanceledException) { /* Shutdown or a stalled client. */ }
                catch (IOException) { /* The launching process may exit at any point. */ }
                finally
                {
                    // IsConnected can already be false after a broken pipe.
                    try { pipe.Disconnect(); } catch (InvalidOperationException) { }
                }
            }
        }
    }

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

    private static async Task<bool> ReceiveAsync(PipeStream pipe, byte[] expected, CancellationToken token)
    {
        var buffer = new byte[expected.Length];
        int count = await pipe.ReadAsync(buffer, token);
        return count == expected.Length && pipe.IsMessageComplete && buffer.AsSpan().SequenceEqual(expected);
    }

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
