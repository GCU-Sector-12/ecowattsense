// BatteryIoctlReader.cs: reads the battery through the Windows battery class driver (IOCTL_BATTERY_QUERY_STATUS),
// and reads the battery percentage and the charger flag through GetSystemPowerStatus.
// Who uses it: SampleCollector, once per sample. Both readers work for a normal user, without administrator rights.
// How it works: find the first battery device path with SetupDi, open it, ask for the battery tag, then for the status.
// Measured on an HP 250 G7: the rate is always "unknown" there, and the capacity falls in steps of about 11 mWh every 10 s.
// The struct fields below are filled by the driver, so the compiler cannot see a write to them (warning CS0649 is switched off).
#pragma warning disable CS0649
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace EnergyCalibrator;

/// <summary>Reads BATTERY_STATUS of the first battery and stores power state, capacity, voltage and rate in a sample.</summary>
internal sealed class BatteryIoctlReader
{
    private const int DigcfPresent = 2;
    private const int DigcfDeviceInterface = 16;
    private const uint IoctlQueryTag = 0x294040;
    private const uint IoctlQueryStatus = 0x29404c;
    private const uint GenericReadWrite = 0x80000000 | 0x40000000;
    private const uint ShareReadWrite = 3;
    private const uint OpenExisting = 3;
    private const uint AttributeNormal = 0x80;
    private const uint UnknownValue = 0xFFFFFFFF;
    private static readonly Guid BatteryClass = new("72631e54-78a4-11d0-bcf7-00aa00b7b32a");
    private static readonly IntPtr InvalidHandle = new(-1);

    /// <summary>The device path of the battery, found once and cleared when the battery cannot be opened.</summary>
    private string? devicePath;

    /// <summary>SP_DEVICE_INTERFACE_DATA of SetupAPI.</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct InterfaceData
    {
        public int Size;
        public Guid Class;
        public int Flags;
        public IntPtr Reserved;
    }

    /// <summary>BATTERY_WAIT_STATUS: asks for the status without waiting for a change.</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct WaitStatus
    {
        public uint Tag;
        public uint Timeout;
        public uint PowerState;
        public uint LowCapacity;
        public uint HighCapacity;
    }

    /// <summary>BATTERY_STATUS: power state flags, capacity in mWh, voltage in mV, rate in mW.</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct BatteryStatus
    {
        public uint PowerState;
        public uint Capacity;
        public uint Voltage;
        public int Rate;
    }

    /// <summary>SetupAPI: opens the set of present devices that have the battery interface. Gives INVALID_HANDLE_VALUE on failure.</summary>
    [DllImport("setupapi.dll", SetLastError = true)]
    private static extern IntPtr SetupDiGetClassDevs(ref Guid classGuid, IntPtr enumerator, IntPtr parent, int flags);

    /// <summary>SetupAPI: gives the interface with the given index from a device set. False when there is none.</summary>
    [DllImport("setupapi.dll", SetLastError = true)]
    private static extern bool SetupDiEnumDeviceInterfaces(IntPtr set, IntPtr deviceInfo, ref Guid classGuid, int index, ref InterfaceData data);

    /// <summary>SetupAPI: gives the device path of an interface. With a null buffer it only reports the size it needs.</summary>
    [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr set, ref InterfaceData data, IntPtr detail, int size, out int needed, IntPtr info);

    /// <summary>SetupAPI: releases a device set.</summary>
    [DllImport("setupapi.dll")]
    private static extern bool SetupDiDestroyDeviceInfoList(IntPtr set);

    /// <summary>Kernel32: opens a device by its path. The returned handle is invalid on failure.</summary>
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern SafeFileHandle CreateFile(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);

    /// <summary>Kernel32: sends the battery tag query (the input is a wait time of 0, the output is the tag).</summary>
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool DeviceIoControl(SafeFileHandle handle, uint code, ref uint input, int inputSize, out uint output, int outputSize, out int returned, IntPtr overlapped);

    /// <summary>Kernel32: sends the battery status query (the input is a BATTERY_WAIT_STATUS, the output a BATTERY_STATUS).</summary>
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool DeviceIoControl(SafeFileHandle handle, uint code, ref WaitStatus input, int inputSize, out BatteryStatus output, int outputSize, out int returned, IntPtr overlapped);

    /// <summary>Reads the battery now and stores the values that are known and believable.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <exception cref="SourceUnavailableException">There is no battery, or a step of the IOCTL failed.</exception>
    public void Read(Sample sample)
    {
        BatteryStatus status = ReadStatus();
        sample.Set(SampleKeys.BatteryState, status.PowerState);
        sample.SetChecked(SampleKeys.BatteryMwh, KnownOrNull(status.Capacity), 1, 1_000_000);
        sample.SetChecked(SampleKeys.BatteryMv, KnownOrNull(status.Voltage), 1000, 100_000);
        sample.SetChecked(SampleKeys.BatteryRateMw, RateOrNull(status.Rate), -500_000, 500_000);
    }

    /// <summary>Turns the driver's "unknown" marker (0xFFFFFFFF) into null.</summary>
    /// <param name="value">Capacity or voltage from the driver.</param>
    /// <returns>The value, or null when the driver says unknown.</returns>
    private static double? KnownOrNull(uint value) => value == UnknownValue ? null : value;

    /// <summary>Turns the driver's "unknown rate" marker (0x80000000) into null.</summary>
    /// <param name="value">Rate from the driver in mW.</param>
    /// <returns>The rate, or null when the driver says unknown.</returns>
    private static double? RateOrNull(int value) => value == int.MinValue ? null : value;

    /// <summary>Opens the battery, asks for its tag and then for its status.</summary>
    /// <returns>The BATTERY_STATUS the driver gave.</returns>
    /// <exception cref="SourceUnavailableException">No battery, or a call failed (the message holds the Win32 error number).</exception>
    private BatteryStatus ReadStatus()
    {
        devicePath ??= FindFirstPath();
        if (devicePath == null) { throw new SourceUnavailableException("no battery found"); }
        using SafeFileHandle handle = CreateFile(devicePath, GenericReadWrite, ShareReadWrite, IntPtr.Zero, OpenExisting, AttributeNormal, IntPtr.Zero);
        if (handle.IsInvalid) { throw OpenFailed(); }
        uint tag = QueryTag(handle);
        return QueryStatus(handle, tag);
    }

    /// <summary>Builds the error for a failed open and forgets the device path so the next sample looks again.</summary>
    /// <returns>The exception to throw.</returns>
    private SourceUnavailableException OpenFailed()
    {
        int error = Marshal.GetLastWin32Error();
        devicePath = null;
        return new SourceUnavailableException($"battery open failed, error {error}");
    }

    /// <summary>Builds the error for a failed IOCTL step. Call it right after the failing call.</summary>
    /// <param name="step">Name of the step, for the message.</param>
    /// <returns>The exception to throw.</returns>
    private static SourceUnavailableException IoctlFailed(string step)
    {
        return new SourceUnavailableException($"battery {step} failed, error {Marshal.GetLastWin32Error()}");
    }

    /// <summary>Asks the driver for the battery tag, which the status call needs.</summary>
    /// <param name="handle">The open battery device.</param>
    /// <returns>The tag.</returns>
    /// <exception cref="SourceUnavailableException">The call failed, or the tag is 0 (no battery in the slot).</exception>
    private static uint QueryTag(SafeFileHandle handle)
    {
        uint wait = 0;
        if (!DeviceIoControl(handle, IoctlQueryTag, ref wait, 4, out uint tag, 4, out _, IntPtr.Zero)) { throw IoctlFailed("tag query"); }
        if (tag == 0) { throw new SourceUnavailableException("no battery present"); }
        return tag;
    }

    /// <summary>Asks the driver for the battery status.</summary>
    /// <param name="handle">The open battery device.</param>
    /// <param name="tag">The tag from QueryTag.</param>
    /// <returns>The status.</returns>
    /// <exception cref="SourceUnavailableException">The call failed.</exception>
    private static BatteryStatus QueryStatus(SafeFileHandle handle, uint tag)
    {
        WaitStatus wait = new() { Tag = tag };
        int inSize = Marshal.SizeOf<WaitStatus>();
        int outSize = Marshal.SizeOf<BatteryStatus>();
        if (!DeviceIoControl(handle, IoctlQueryStatus, ref wait, inSize, out BatteryStatus status, outSize, out _, IntPtr.Zero)) { throw IoctlFailed("status query"); }
        return status;
    }

    /// <summary>Finds the device path of the first battery with SetupDi.</summary>
    /// <returns>The path, or null when there is no battery device.</returns>
    private static string? FindFirstPath()
    {
        Guid classGuid = BatteryClass;
        IntPtr set = SetupDiGetClassDevs(ref classGuid, IntPtr.Zero, IntPtr.Zero, DigcfPresent | DigcfDeviceInterface);
        if (set == InvalidHandle) { return null; }
        try { return PathFromSet(set, ref classGuid); }
        finally { SetupDiDestroyDeviceInfoList(set); }
    }

    /// <summary>Takes the first interface of the device set and reads its path.</summary>
    /// <param name="set">The SetupDi device set.</param>
    /// <param name="classGuid">The battery class GUID.</param>
    /// <returns>The path, or null when the set is empty.</returns>
    private static string? PathFromSet(IntPtr set, ref Guid classGuid)
    {
        InterfaceData data = new() { Size = Marshal.SizeOf<InterfaceData>() };
        if (!SetupDiEnumDeviceInterfaces(set, IntPtr.Zero, ref classGuid, 0, ref data)) { return null; }
        // The first call is meant to fail: it only tells how big the detail buffer must be.
        SetupDiGetDeviceInterfaceDetail(set, ref data, IntPtr.Zero, 0, out int needed, IntPtr.Zero);
        IntPtr detail = Marshal.AllocHGlobal(Math.Max(needed, 8));
        try { return ReadDetailPath(set, ref data, detail, needed); }
        finally { Marshal.FreeHGlobal(detail); }
    }

    /// <summary>Fills the detail buffer and reads the path from it.</summary>
    /// <param name="set">The SetupDi device set.</param>
    /// <param name="data">The interface data.</param>
    /// <param name="detail">Buffer of at least the needed size.</param>
    /// <param name="needed">Size of the buffer in bytes.</param>
    /// <returns>The path, or null when the call fails.</returns>
    private static string? ReadDetailPath(IntPtr set, ref InterfaceData data, IntPtr detail, int needed)
    {
        // The detail struct starts with its own size: 8 in a 64-bit process, 6 in a 32-bit process (a SetupAPI rule).
        Marshal.WriteInt32(detail, IntPtr.Size == 8 ? 8 : 6);
        bool ok = SetupDiGetDeviceInterfaceDetail(set, ref data, detail, needed, out _, IntPtr.Zero);
        return ok ? Marshal.PtrToStringUni(detail + 4) : null;
    }
}

/// <summary>Reads the battery percentage and the charger flag with GetSystemPowerStatus (needs no rights).</summary>
internal sealed class PowerStatusReader
{
    /// <summary>Reads the power status and stores the percentage and the charger flag when they are known.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <exception cref="SourceUnavailableException">The call failed.</exception>
    public void Read(Sample sample)
    {
        if (!NativeMethods.GetSystemPowerStatus(out NativeMethods.SystemPowerStatus status))
        {
            throw new SourceUnavailableException($"GetSystemPowerStatus failed, error {Marshal.GetLastWin32Error()}");
        }
        sample.SetChecked(SampleKeys.BatteryPercent, KnownOrNull(status.BatteryLifePercent), 0, 100);
        sample.SetChecked(SampleKeys.AcLine, KnownOrNull(status.AcLineStatus), 0, 1);
    }

    /// <summary>Turns the value 255 ("unknown" in this structure) into null.</summary>
    /// <param name="value">A byte from SYSTEM_POWER_STATUS.</param>
    /// <returns>The value, or null for 255.</returns>
    private static double? KnownOrNull(byte value) => value == 255 ? null : value;
}
