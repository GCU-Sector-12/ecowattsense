// WmiReaders.cs: everything that reads or sets values through WMI (Windows Management Instrumentation).
// Who uses it: SampleCollector (battery, Intel DTT, brightness), TestRunner (sets the brightness), MainWindow (machine info).
// How it works: WmiHelper runs a query and copies the rows into plain dictionaries, so no WMI object stays open.
// The Intel DTT class (root\wmi EsifDeviceInformation) is undocumented. What its participants mean was derived from a test
// on an HP 250 G7: Power is in tenths of a watt; participant 3 behaves like the processor package, participant 4 like its cores.
using System.Globalization;
using System.Management;

namespace EnergyCalibrator;

/// <summary>Small helpers to run WMI queries and to read values from the rows.</summary>
internal static class WmiHelper
{
    /// <summary>Runs one WQL query and copies every row into a dictionary.</summary>
    /// <param name="scope">The WMI namespace, for example root\wmi.</param>
    /// <param name="wql">The query text.</param>
    /// <returns>The rows. An empty list means the class exists but has no instance.</returns>
    /// <exception cref="ManagementException">The class or namespace is missing, or the query failed.</exception>
    /// <exception cref="UnauthorizedAccessException">Access to the class is denied for this user.</exception>
    public static List<Dictionary<string, object?>> Query(string scope, string wql)
    {
        using ManagementObjectSearcher searcher = new(scope, wql);
        using ManagementObjectCollection results = searcher.Get();
        List<Dictionary<string, object?>> rows = new();
        foreach (ManagementBaseObject row in results) { rows.Add(CopyRow(row)); }
        return rows;
    }

    /// <summary>Copies the properties of one WMI object into a dictionary and releases the object.</summary>
    /// <param name="row">The WMI object.</param>
    /// <returns>Property name to value.</returns>
    private static Dictionary<string, object?> CopyRow(ManagementBaseObject row)
    {
        Dictionary<string, object?> copy = new();
        foreach (PropertyData property in row.Properties) { copy[property.Name] = property.Value; }
        row.Dispose();
        return copy;
    }

    /// <summary>Reads a numeric property.</summary>
    /// <param name="row">A row from Query.</param>
    /// <param name="key">The property name.</param>
    /// <returns>The value, or null when the property is missing or not a number.</returns>
    public static double? ToDouble(Dictionary<string, object?> row, string key)
    {
        if (!row.TryGetValue(key, out object? value)) { return null; }
        if (value is not IConvertible convertible) { return null; }
        return Convert.ToDouble(convertible, CultureInfo.InvariantCulture);
    }

    /// <summary>Reads a Boolean property as 1 or 0.</summary>
    /// <param name="row">A row from Query.</param>
    /// <param name="key">The property name.</param>
    /// <returns>1 for true, 0 for false, null when the property is missing or not a Boolean.</returns>
    public static double? ToFlag(Dictionary<string, object?> row, string key)
    {
        if (!row.TryGetValue(key, out object? value)) { return null; }
        if (value is not bool flag) { return null; }
        return flag ? 1.0 : 0.0;
    }

    /// <summary>Tells whether a Boolean property is true.</summary>
    /// <param name="row">A row from Query.</param>
    /// <param name="key">The property name.</param>
    /// <returns>True only when the property exists and is true.</returns>
    public static bool IsTrue(Dictionary<string, object?> row, string key) => row.TryGetValue(key, out object? value) && value is true;

    /// <summary>Reads a text property.</summary>
    /// <param name="row">A row from Query.</param>
    /// <param name="key">The property name.</param>
    /// <returns>The text, or an empty string when the property is missing.</returns>
    public static string Text(Dictionary<string, object?> row, string key)
    {
        if (!row.TryGetValue(key, out object? value)) { return ""; }
        return Convert.ToString(value, CultureInfo.InvariantCulture) ?? "";
    }
}

/// <summary>Reads root\wmi BatteryStatus. It is a second source for the battery charge and lags the IOCTL by about one 10 s step.</summary>
internal sealed class WmiBatteryReader
{
    /// <summary>Reads the first battery instance and stores charge, voltage and the charger flag.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <exception cref="SourceUnavailableException">WMI shows no battery.</exception>
    public void Read(Sample sample)
    {
        List<Dictionary<string, object?>> rows = WmiHelper.Query(@"root\wmi", "SELECT RemainingCapacity, Voltage, PowerOnline FROM BatteryStatus");
        if (rows.Count == 0) { throw new SourceUnavailableException("WMI shows no battery"); }
        Dictionary<string, object?> row = rows[0];
        sample.SetChecked(SampleKeys.WmiMwh, WmiHelper.ToDouble(row, "RemainingCapacity"), 1, 1_000_000);
        sample.SetChecked(SampleKeys.WmiMv, WmiHelper.ToDouble(row, "Voltage"), 1000, 100_000);
        sample.SetChecked(SampleKeys.WmiOnline, WmiHelper.ToFlag(row, "PowerOnline"), 0, 1);
    }
}

/// <summary>
/// Reads Intel Dynamic Tuning (root\wmi EsifDeviceInformation). It needs administrator rights.
/// Every participant is stored by the number at the end of its instance name, never by the full name (which can hold ids).
/// </summary>
internal sealed class DttReader
{
    /// <summary>Reads all participants and stores their power (watts) and temperature (degrees Celsius).</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <exception cref="SourceUnavailableException">Intel DTT answers with no participant.</exception>
    /// <exception cref="UnauthorizedAccessException">The program is not running as administrator.</exception>
    public void Read(Sample sample)
    {
        List<Dictionary<string, object?>> rows = WmiHelper.Query(@"root\wmi", "SELECT InstanceName, Power, Temperature FROM EsifDeviceInformation");
        if (rows.Count == 0) { throw new SourceUnavailableException("Intel DTT reports no participants"); }
        List<string> usedIds = new();
        for (int i = 0; i < rows.Count; i++) { StoreParticipant(sample, rows[i], i, usedIds); }
    }

    /// <summary>Stores power and temperature of one participant.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <param name="row">The WMI row.</param>
    /// <param name="ordinal">Position of the row, used when the instance name has no usable number.</param>
    /// <param name="usedIds">Ids already taken in this sample; the method adds the new one.</param>
    private static void StoreParticipant(Sample sample, Dictionary<string, object?> row, int ordinal, List<string> usedIds)
    {
        string id = ParticipantId(row, ordinal, usedIds);
        usedIds.Add(id);
        // Power is in tenths of a watt (derived from a test, not documented by Intel).
        sample.SetChecked($"{SampleKeys.DttPrefix}{id}_power_w", WmiHelper.ToDouble(row, "Power") / 10.0, 0, 200);
        sample.SetChecked($"{SampleKeys.DttPrefix}{id}_temp_c", WmiHelper.ToDouble(row, "Temperature"), -20, 150);
    }

    /// <summary>Gives the participant id: the digits after the last underscore of the instance name, or "x" and the position.</summary>
    /// <param name="row">The WMI row.</param>
    /// <param name="ordinal">Position of the row.</param>
    /// <param name="usedIds">Ids already taken in this sample.</param>
    /// <returns>A short id that holds only letters and digits.</returns>
    private static string ParticipantId(Dictionary<string, object?> row, int ordinal, List<string> usedIds)
    {
        string name = WmiHelper.Text(row, "InstanceName");
        int cut = name.LastIndexOf('_');
        string suffix = cut >= 0 ? name[(cut + 1)..] : "";
        bool usable = suffix.Length > 0 && suffix.All(char.IsDigit) && !usedIds.Contains(suffix);
        return usable ? suffix : $"x{ordinal}";
    }
}

/// <summary>Reads and sets the screen brightness through WMI (root\wmi WmiMonitorBrightness and WmiMonitorBrightnessMethods).</summary>
internal sealed class BrightnessControl
{
    private const string Scope = @"root\wmi";

    /// <summary>Reads the brightness of the active display and stores it in a sample.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <exception cref="SourceUnavailableException">No display reports a brightness (a desktop PC or an external monitor).</exception>
    public void Read(Sample sample)
    {
        sample.SetChecked(SampleKeys.Brightness, ReadPercent(), 0, 100);
    }

    /// <summary>Reads the brightness of the active display now.</summary>
    /// <returns>The brightness in percent.</returns>
    /// <exception cref="SourceUnavailableException">No active display reports a brightness.</exception>
    public int ReadPercent()
    {
        List<Dictionary<string, object?>> rows = WmiHelper.Query(Scope, "SELECT Active, CurrentBrightness FROM WmiMonitorBrightness");
        Dictionary<string, object?>? active = rows.FirstOrDefault(row => WmiHelper.IsTrue(row, "Active"));
        if (active == null) { throw new SourceUnavailableException("no display reports its brightness"); }
        return (int)(WmiHelper.ToDouble(active, "CurrentBrightness") ?? throw new SourceUnavailableException("brightness value missing"));
    }

    /// <summary>Reads the brightness and gives null instead of throwing.</summary>
    /// <returns>The brightness in percent, or null when it cannot be read.</returns>
    public int? TryReadPercent()
    {
        try { return ReadPercent(); }
        catch (Exception) { return null; }
    }

    /// <summary>Sets the brightness of every display that accepts it, with a time-out of 1 second as the WMI method needs.</summary>
    /// <param name="percent">Brightness 0 to 100.</param>
    /// <returns>Null when it worked, otherwise a short reason. It never throws.</returns>
    public string? TrySetPercent(int percent)
    {
        try { SetPercent(percent); return null; }
        catch (Exception ex) { return Reasons.From(ex); }
    }

    /// <summary>Calls WmiSetBrightness on every display instance.</summary>
    /// <param name="percent">Brightness 0 to 100.</param>
    /// <exception cref="SourceUnavailableException">No display accepts a brightness change.</exception>
    private static void SetPercent(int percent)
    {
        using ManagementObjectSearcher searcher = new(Scope, "SELECT * FROM WmiMonitorBrightnessMethods");
        using ManagementObjectCollection results = searcher.Get();
        int applied = 0;
        foreach (ManagementObject method in results) { InvokeSet(method, percent); applied++; }
        if (applied == 0) { throw new SourceUnavailableException("no display accepts brightness changes"); }
    }

    /// <summary>Calls WmiSetBrightness(timeout 1, brightness) on one display instance and releases it.</summary>
    /// <param name="method">The WMI object of one display.</param>
    /// <param name="percent">Brightness 0 to 100.</param>
    private static void InvokeSet(ManagementObject method, int percent)
    {
        method.InvokeMethod("WmiSetBrightness", new object[] { 1U, (byte)percent });
        method.Dispose();
    }
}

/// <summary>Maker, model and processor of this computer. It holds no computer name, user name, serial number or device id.</summary>
/// <param name="Maker">Manufacturer from Win32_ComputerSystem.</param>
/// <param name="Model">Model from Win32_ComputerSystem.</param>
/// <param name="Cpu">Processor name from Win32_Processor.</param>
/// <param name="LogicalProcessors">Number of logical processors.</param>
/// <param name="Windows">Windows version number.</param>
internal sealed record MachineInfo(string Maker, string Model, string Cpu, int LogicalProcessors, string Windows)
{
    /// <summary>Used until the real values are read, and when they cannot be read.</summary>
    public static MachineInfo Unknown { get; } = new("unknown", "unknown", "unknown", Environment.ProcessorCount, Environment.OSVersion.Version.ToString());

    /// <summary>Reads maker, model and processor from WMI.</summary>
    /// <returns>The machine info; the Unknown value when WMI fails. It never throws.</returns>
    public static MachineInfo Read()
    {
        try { return ReadCore(); }
        catch (Exception) { return Unknown; }
    }

    /// <summary>Runs the two WMI queries.</summary>
    /// <returns>The machine info.</returns>
    private static MachineInfo ReadCore()
    {
        Dictionary<string, object?>? system = WmiHelper.Query(@"root\cimv2", "SELECT Manufacturer, Model FROM Win32_ComputerSystem").FirstOrDefault();
        Dictionary<string, object?>? cpu = WmiHelper.Query(@"root\cimv2", "SELECT Name FROM Win32_Processor").FirstOrDefault();
        return new MachineInfo(Clean(system, "Manufacturer"), Clean(system, "Model"), Clean(cpu, "Name"), Environment.ProcessorCount, Environment.OSVersion.Version.ToString());
    }

    /// <summary>Reads one text value and replaces an empty one with "unknown".</summary>
    /// <param name="row">The WMI row, or null.</param>
    /// <param name="key">The property name.</param>
    /// <returns>The trimmed text, or "unknown".</returns>
    private static string Clean(Dictionary<string, object?>? row, string key)
    {
        if (row == null) { return "unknown"; }
        string text = WmiHelper.Text(row, key).Trim();
        return text.Length == 0 ? "unknown" : text;
    }
}
