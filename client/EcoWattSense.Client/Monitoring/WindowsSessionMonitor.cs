using System.ComponentModel;
using System.Runtime.InteropServices;

namespace EcoWattSense.Client.Monitoring;

/// <summary>Queries the session containing this process, including an RDP user session.</summary>
public sealed class WindowsSessionMonitor
{
    private const uint CurrentSession = 0xFFFFFFFF;
    private const int SessionInfoEx = 25;

    /// <returns>
    /// True when locked, false when unlocked, or null when Windows reports an unknown
    /// state or this process is not in an interactive user session.
    /// </returns>
    /// <exception cref="InvalidOperationException">The Windows query failed or returned invalid data.</exception>
    public bool? IsLocked()
    {
        if (!OperatingSystem.IsWindows())
        {
            throw new PlatformNotSupportedException("Session-lock monitoring requires Windows.");
        }

        IntPtr buffer = IntPtr.Zero;
        try
        {
            if (!WTSQuerySessionInformationW(IntPtr.Zero, CurrentSession, SessionInfoEx,
                    out buffer, out uint bytesReturned))
            {
                int error = Marshal.GetLastWin32Error();
                throw new InvalidOperationException("Unable to query Windows session-lock status.",
                    new Win32Exception(error));
            }

            if (buffer == IntPtr.Zero || bytesReturned < Marshal.SizeOf<SessionInfo>())
            {
                throw new InvalidOperationException("Windows returned incomplete session information.");
            }

            // Check the discriminator before interpreting the level-1 union payload.
            if (Marshal.ReadInt32(buffer) != 1)
            {
                return null;
            }

            var info = Marshal.PtrToStructure<SessionInfo>(buffer).Data;
            // Session 0 belongs to services; it must not be reported as an unlocked user.
            if (info.SessionId == 0 || string.IsNullOrWhiteSpace(info.UserName))
            {
                return null;
            }

            return info.SessionFlags switch
            {
                0 => true,  // WTS_SESSIONSTATE_LOCK
                1 => false, // WTS_SESSIONSTATE_UNLOCK
                _ => null   // Includes WTS_SESSIONSTATE_UNKNOWN (-1).
            };
        }
        finally
        {
            if (buffer != IntPtr.Zero)
            {
                WTSFreeMemory(buffer);
            }
        }
    }

    // Native WTSINFOEXW and WTSINFOEX_LEVEL1_W. Keep all fields, including the
    // 64-bit timestamps, so the CLR preserves the native union's 8-byte alignment.
    [StructLayout(LayoutKind.Sequential)]
    private struct SessionInfo
    {
        public uint Level;
        public SessionInfoLevel1 Data;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct SessionInfoLevel1
    {
        public uint SessionId;
        public int SessionState;
        public int SessionFlags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 33)]
        public string WinStationName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 21)]
        public string UserName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 18)]
        public string DomainName;
        public long LogonTime;
        public long ConnectTime;
        public long DisconnectTime;
        public long LastInputTime;
        public long CurrentTime;
        public uint IncomingBytes;
        public uint OutgoingBytes;
        public uint IncomingFrames;
        public uint OutgoingFrames;
        public uint IncomingCompressedBytes;
        public uint OutgoingCompressedBytes;
    }

    [DllImport("wtsapi32.dll", ExactSpelling = true, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool WTSQuerySessionInformationW(IntPtr server, uint sessionId,
        int infoClass, out IntPtr buffer, out uint bytesReturned);

    [DllImport("wtsapi32.dll", ExactSpelling = true)]
    private static extern void WTSFreeMemory(IntPtr memory);
}
