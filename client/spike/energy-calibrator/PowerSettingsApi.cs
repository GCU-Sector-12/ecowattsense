// PowerSettingsApi.cs: reads and writes four processor power settings of the active power plan, for use on battery (DC), with the
// documented functions of powrprof.dll (powersetting.h). Also reads the Windows 11 "Power mode" overlay.
// Who uses it: SampleCollector (the live row), PowerSettingsSession (the change before the test and the restore afterwards).
// The four settings are in the subgroup "Processor power management":
//   Processor energy performance preference (PERFEPP, 0 = favour speed, 100 = favour saving),
//   Processor performance boost mode (PERFBOOSTMODE, 2 = Aggressive),
//   Maximum processor state (PROCTHROTTLEMAX, percent) and Minimum processor state (PROCTHROTTLEMIN, percent).
// The overlay is only read. Changing it needs an undocumented API, which this program does not use.
// Every function here returns a Win32 error code; 0 means success.
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace EnergyCalibrator;

/// <summary>The documented powrprof.dll and kernel32.dll functions used for the power settings.</summary>
internal static class PowerNative
{
    /// <summary>Gives the GUID of the active power plan. The caller must free the returned memory with LocalFree.</summary>
    [DllImport("powrprof.dll")]
    internal static extern uint PowerGetActiveScheme(IntPtr userRootPowerKey, out IntPtr activePolicyGuid);

    /// <summary>Reads the battery (DC) value of one setting of one plan.</summary>
    [DllImport("powrprof.dll")]
    internal static extern uint PowerReadDCValueIndex(IntPtr rootPowerKey, ref Guid scheme, ref Guid subgroup, ref Guid setting, out uint value);

    /// <summary>Writes the battery (DC) value of one setting of one plan. The plan must be activated again for the change to apply.</summary>
    [DllImport("powrprof.dll")]
    internal static extern uint PowerWriteDCValueIndex(IntPtr rootPowerKey, ref Guid scheme, ref Guid subgroup, ref Guid setting, uint value);

    /// <summary>Makes a plan the active plan. Called on the plan that is already active, it applies the changed values.</summary>
    [DllImport("powrprof.dll")]
    internal static extern uint PowerSetActiveScheme(IntPtr userRootPowerKey, ref Guid scheme);

    /// <summary>Gives the Windows 11 "Power mode" overlay that is in effect. All zeros is the default (Balanced).</summary>
    [DllImport("powrprof.dll")]
    internal static extern uint PowerGetEffectiveOverlayScheme(out Guid effectiveOverlayPolicyGuid);

    /// <summary>Frees memory that Windows allocated, such as the plan GUID from PowerGetActiveScheme.</summary>
    [DllImport("kernel32.dll")]
    internal static extern IntPtr LocalFree(IntPtr memory);
}

/// <summary>The GUIDs of the processor subgroup and of the four settings, and the values used for "maximum performance".</summary>
internal static class PowerIds
{
    /// <summary>Subgroup "Processor power management" (SUB_PROCESSOR).</summary>
    public static readonly Guid ProcessorSubgroup = new("54533251-82be-4824-96c1-47b60b740d00");

    /// <summary>Processor energy performance preference (PERFEPP).</summary>
    public static readonly Guid EnergyPreference = new("36687f9e-e3a5-4dbf-b1dc-15eb381c6863");

    /// <summary>Processor performance boost mode (PERFBOOSTMODE).</summary>
    public static readonly Guid BoostMode = new("be337238-0d82-4146-a960-4f3749d470c7");

    /// <summary>Maximum processor state (PROCTHROTTLEMAX).</summary>
    public static readonly Guid MaximumState = new("bc5038f7-23e0-4960-96da-33abaf5935ec");

    /// <summary>Minimum processor state (PROCTHROTTLEMIN).</summary>
    public static readonly Guid MinimumState = new("893dee8e-2bef-41e0-89c6-b55d0929964c");

    /// <summary>Energy performance preference for the test: 0 favours speed.</summary>
    public const uint TestEnergyPreference = 0;

    /// <summary>Boost mode for the test: 2 is Aggressive.</summary>
    public const uint TestBoostMode = 2;

    /// <summary>Maximum processor state for the test, in percent.</summary>
    public const uint TestMaximumState = 100;
}

/// <summary>Plain-English names for boost modes and power mode overlays.</summary>
internal static class PowerNames
{
    private static readonly Guid EfficiencyOverlay = new("961cc777-2547-4f9d-8174-7d86181b8a7a");
    private static readonly Guid BetterPerformanceOverlay = new("3af9b8d9-7c97-431d-ad78-34a8bfea439f");
    private static readonly Guid BestPerformanceOverlay = new("ded574b5-45a0-4f42-8737-46345c09c238");
    private static readonly string[] BoostNames =
    [
        "Disabled", "Enabled", "Aggressive", "Efficient Enabled", "Efficient Aggressive", "Aggressive At Guaranteed", "Efficient Aggressive At Guaranteed"
    ];
    private static readonly string[] OverlayNames = ["Balanced", "Best power efficiency", "Better performance", "Best performance", "other"];

    /// <summary>Gives the name of a boost mode.</summary>
    /// <param name="mode">The PERFBOOSTMODE value.</param>
    /// <returns>For example "Aggressive", or "mode 9" for a value Windows does not list.</returns>
    public static string Boost(uint mode) => mode < BoostNames.Length ? BoostNames[mode] : $"mode {mode}";

    /// <summary>Turns an overlay GUID into a small code that fits in a sample: 0 Balanced, 1 efficiency, 2 better, 3 best, 4 other.</summary>
    /// <param name="overlay">The overlay GUID.</param>
    /// <returns>The code.</returns>
    public static int OverlayCode(Guid overlay)
    {
        if (overlay == Guid.Empty) { return 0; }
        if (overlay == EfficiencyOverlay) { return 1; }
        if (overlay == BetterPerformanceOverlay) { return 2; }
        if (overlay == BestPerformanceOverlay) { return 3; }
        return 4;
    }

    /// <summary>Gives the name of an overlay code.</summary>
    /// <param name="code">A code from OverlayCode.</param>
    /// <returns>The name.</returns>
    public static string OverlayFromCode(int code) => code >= 0 && code < OverlayNames.Length ? OverlayNames[code] : "unknown";

    /// <summary>Gives the name of an overlay.</summary>
    /// <param name="overlay">The overlay GUID, or null when it could not be read.</param>
    /// <returns>The name, or "unknown".</returns>
    public static string Overlay(Guid? overlay) => overlay.HasValue ? OverlayFromCode(OverlayCode(overlay.Value)) : "unknown";
}

/// <summary>The four battery settings of one power plan and the overlay at the moment they were read. It is also the backup file content.</summary>
/// <param name="Scheme">GUID of the power plan.</param>
/// <param name="Epp">Processor energy performance preference on battery, 0 to 100.</param>
/// <param name="BoostMode">Processor performance boost mode on battery.</param>
/// <param name="MaxPercent">Maximum processor state on battery, in percent.</param>
/// <param name="MinPercent">Minimum processor state on battery, in percent.</param>
/// <param name="Overlay">The Windows 11 power mode overlay, or null when it could not be read. Only recorded, never changed.</param>
internal sealed record PowerSnapshot(Guid Scheme, uint Epp, uint BoostMode, uint MaxPercent, uint MinPercent, Guid? Overlay)
{
    /// <summary>Gives the settings that the test uses: the same plan with maximum performance. The minimum state is kept.</summary>
    /// <returns>The snapshot of the test settings.</returns>
    public PowerSnapshot ForTest() => this with { Epp = PowerIds.TestEnergyPreference, BoostMode = PowerIds.TestBoostMode, MaxPercent = PowerIds.TestMaximumState };

    /// <summary>Tells whether the plan and the four values are the same. The overlay is not compared.</summary>
    /// <param name="other">The other snapshot.</param>
    /// <returns>True when plan and values match.</returns>
    public bool SameValues(PowerSnapshot other)
    {
        bool sameBoost = BoostMode == other.BoostMode && MaxPercent == other.MaxPercent && MinPercent == other.MinPercent;
        return Scheme == other.Scheme && Epp == other.Epp && sameBoost;
    }

    /// <summary>Describes the values in one line. The plan GUID is left out, so it never reaches the result files.</summary>
    /// <returns>For example "EPP 50, boost Aggressive, max 100%, min 5%, power mode Balanced".</returns>
    public string Describe()
    {
        return $"EPP {Epp}, boost {PowerNames.Boost(BoostMode)}, max {MaxPercent}%, min {MinPercent}%, power mode {PowerNames.Overlay(Overlay)}";
    }

    /// <summary>Gives the values for model.json. The plan GUID is left out.</summary>
    /// <returns>An object that the JSON serializer writes with snake_case names.</returns>
    public object ToDocument()
    {
        return new
        {
            energy_performance_preference = Epp,
            boost_mode = BoostMode,
            boost_mode_name = PowerNames.Boost(BoostMode),
            max_state_pct = MaxPercent,
            min_state_pct = MinPercent,
            power_mode = PowerNames.Overlay(Overlay)
        };
    }
}

/// <summary>Reads the power settings. Works for a normal user.</summary>
internal static class PowerSettingsReader
{
    /// <summary>Reads the active plan and stores its four battery values and the overlay in a sample.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <exception cref="SourceUnavailableException">A call failed (the message holds the Win32 error number).</exception>
    public static void Read(Sample sample)
    {
        PowerSnapshot snapshot = ReadActive();
        sample.SetChecked(SampleKeys.PowerEpp, snapshot.Epp, 0, 100);
        sample.SetChecked(SampleKeys.PowerBoost, snapshot.BoostMode, 0, 20);
        sample.SetChecked(SampleKeys.PowerMax, snapshot.MaxPercent, 0, 100);
        sample.SetChecked(SampleKeys.PowerMin, snapshot.MinPercent, 0, 100);
        if (snapshot.Overlay.HasValue) { sample.Set(SampleKeys.PowerOverlay, PowerNames.OverlayCode(snapshot.Overlay.Value)); }
    }

    /// <summary>Reads the values of the plan that is active now.</summary>
    /// <returns>The snapshot.</returns>
    /// <exception cref="SourceUnavailableException">A call failed.</exception>
    public static PowerSnapshot ReadActive() => ReadScheme(ActiveScheme());

    /// <summary>Reads the values of one plan.</summary>
    /// <param name="scheme">GUID of the plan.</param>
    /// <returns>The snapshot, with the overlay that is in effect now.</returns>
    /// <exception cref="SourceUnavailableException">A call failed.</exception>
    public static PowerSnapshot ReadScheme(Guid scheme)
    {
        uint epp = ReadDc(scheme, PowerIds.EnergyPreference);
        uint boost = ReadDc(scheme, PowerIds.BoostMode);
        uint max = ReadDc(scheme, PowerIds.MaximumState);
        uint min = ReadDc(scheme, PowerIds.MinimumState);
        return new PowerSnapshot(scheme, epp, boost, max, min, TryReadOverlay());
    }

    /// <summary>Gives the GUID of the active plan.</summary>
    /// <returns>The GUID.</returns>
    /// <exception cref="SourceUnavailableException">The call failed.</exception>
    public static Guid ActiveScheme()
    {
        uint error = PowerNative.PowerGetActiveScheme(IntPtr.Zero, out IntPtr pointer);
        if (error != 0) { throw new SourceUnavailableException($"the active power plan could not be read, error {error}"); }
        try { return GuidAt(pointer); }
        finally { PowerNative.LocalFree(pointer); }
    }

    /// <summary>Copies a GUID out of unmanaged memory.</summary>
    /// <param name="pointer">Address of the 16 bytes.</param>
    /// <returns>The GUID.</returns>
    private static Guid GuidAt(IntPtr pointer)
    {
        byte[] bytes = new byte[16];
        Marshal.Copy(pointer, bytes, 0, bytes.Length);
        return new Guid(bytes);
    }

    /// <summary>Reads one battery value of the processor subgroup.</summary>
    /// <param name="scheme">GUID of the plan.</param>
    /// <param name="setting">GUID of the setting.</param>
    /// <returns>The value.</returns>
    /// <exception cref="SourceUnavailableException">The call failed.</exception>
    private static uint ReadDc(Guid scheme, Guid setting)
    {
        Guid plan = scheme;
        Guid subgroup = PowerIds.ProcessorSubgroup;
        Guid id = setting;
        uint error = PowerNative.PowerReadDCValueIndex(IntPtr.Zero, ref plan, ref subgroup, ref id, out uint value);
        if (error != 0) { throw new SourceUnavailableException($"a power setting could not be read, error {error}"); }
        return value;
    }

    /// <summary>Reads the overlay. Older Windows versions have no such function, and then the overlay is simply unknown.</summary>
    /// <returns>The overlay GUID, or null.</returns>
    private static Guid? TryReadOverlay()
    {
        try { return ReadOverlay(); }
        catch (Exception) { return null; }
    }

    /// <summary>Calls PowerGetEffectiveOverlayScheme.</summary>
    /// <returns>The overlay GUID, or null when the call reports an error.</returns>
    private static Guid? ReadOverlay()
    {
        uint error = PowerNative.PowerGetEffectiveOverlayScheme(out Guid overlay);
        if (error != 0) { return null; }
        return overlay;
    }
}

/// <summary>Writes the power settings. A normal user can usually write the plan of their own account; if not, the error is reported.</summary>
internal static class PowerSettingsWriter
{
    /// <summary>Writes the test values (energy preference, boost mode, maximum state) into the plan and applies them. The minimum state is not touched.</summary>
    /// <param name="original">The snapshot taken before the change; only its plan GUID is used.</param>
    /// <exception cref="SourceUnavailableException">A write or the activation failed. Some values may already be written.</exception>
    public static void ApplyTestValues(PowerSnapshot original)
    {
        WriteDc(original.Scheme, PowerIds.EnergyPreference, PowerIds.TestEnergyPreference);
        WriteDc(original.Scheme, PowerIds.BoostMode, PowerIds.TestBoostMode);
        WriteDc(original.Scheme, PowerIds.MaximumState, PowerIds.TestMaximumState);
        Activate(original.Scheme);
    }

    /// <summary>Writes the four original values back into their plan. The plan is activated again only when it is still the active one.</summary>
    /// <param name="original">The snapshot taken before the change.</param>
    /// <exception cref="SourceUnavailableException">A write or the activation failed.</exception>
    public static void PutBack(PowerSnapshot original)
    {
        WriteDc(original.Scheme, PowerIds.EnergyPreference, original.Epp);
        WriteDc(original.Scheme, PowerIds.BoostMode, original.BoostMode);
        WriteDc(original.Scheme, PowerIds.MaximumState, original.MaxPercent);
        WriteDc(original.Scheme, PowerIds.MinimumState, original.MinPercent);
        // When the user chose another plan meanwhile, the values are stored and apply the next time that plan is chosen.
        // Activating the old plan again would switch the user's plan back, which the test must not do.
        if (PowerSettingsReader.ActiveScheme() == original.Scheme) { Activate(original.Scheme); }
    }

    /// <summary>Writes one battery value of the processor subgroup.</summary>
    /// <param name="scheme">GUID of the plan.</param>
    /// <param name="setting">GUID of the setting.</param>
    /// <param name="value">The new value.</param>
    /// <exception cref="SourceUnavailableException">The call failed.</exception>
    private static void WriteDc(Guid scheme, Guid setting, uint value)
    {
        Guid plan = scheme;
        Guid subgroup = PowerIds.ProcessorSubgroup;
        Guid id = setting;
        uint error = PowerNative.PowerWriteDCValueIndex(IntPtr.Zero, ref plan, ref subgroup, ref id, value);
        if (error != 0) { throw Failed("a power setting could not be written", error); }
    }

    /// <summary>Activates a plan, which makes Windows use the values just written.</summary>
    /// <param name="scheme">GUID of the plan.</param>
    /// <exception cref="SourceUnavailableException">The call failed.</exception>
    private static void Activate(Guid scheme)
    {
        Guid plan = scheme;
        uint error = PowerNative.PowerSetActiveScheme(IntPtr.Zero, ref plan);
        if (error != 0) { throw Failed("the power plan could not be applied", error); }
    }

    /// <summary>Builds the error for a failed call, with the Windows text for the error code (for example "Access is denied").</summary>
    /// <param name="what">What failed.</param>
    /// <param name="error">The Win32 error code.</param>
    /// <returns>The exception to throw.</returns>
    private static SourceUnavailableException Failed(string what, uint error)
    {
        string text = new Win32Exception((int)error).Message;
        return new SourceUnavailableException($"{what}: {text} (error {error})");
    }
}
