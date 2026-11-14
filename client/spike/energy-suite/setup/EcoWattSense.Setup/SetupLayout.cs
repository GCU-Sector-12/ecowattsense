// SetupLayout.cs: the fixed names and paths of the installed suite (SPEC 2.2, 5.1) and the log of Setup.
// Who uses it: both engines, the plan builders and the window. Tests create a layout with temporary folders.
namespace EcoWattSense.Setup;

/// <summary>Names and folders of the installation.</summary>
/// <param name="ProgramDir">Where the programs go, normally C:\Program Files\EcoWattSense.</param>
/// <param name="DataDir">The data folder, normally C:\ProgramData\EcoWattSense.</param>
public sealed record SetupLayout(string ProgramDir, string DataDir)
{
    /// <summary>The service name.</summary>
    public const string ServiceName = "EcoWattSenseService";

    /// <summary>The name shown in the Services list.</summary>
    public const string ServiceDisplayName = "EcoWattSense Energy Service";

    /// <summary>The account of the service (SPEC 5.1).</summary>
    public const string ServiceAccount = @"NT AUTHORITY\LocalService";

    /// <summary>The Task Scheduler task of the client autostart; the classic client uses the same name.</summary>
    public const string TaskName = "EcoWattSense Client";

    /// <summary>The seven data sub-folders the service owns; Client is never part of the list.</summary>
    public static readonly string[] ServiceFolders = ["scan", "calibration", "history", "status", "queue", "config", "logs"];

    /// <summary>The standard layout on a real computer.</summary>
    public static SetupLayout Standard => new(
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "EcoWattSense"),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "EcoWattSense"));

    /// <summary>The installed service program.</summary>
    public string ServiceExe => Path.Combine(ProgramDir, "ews-service.exe");

    /// <summary>The installed client.</summary>
    public string ClientExe => Path.Combine(ProgramDir, "EcoWattSense.Client.Gui.exe");

    /// <summary>The installed calibrator.</summary>
    public string CalibratorExe => Path.Combine(ProgramDir, "EnergyCalibrator.exe");

    /// <summary>A data sub-folder.</summary>
    public string Data(string name) => Path.Combine(DataDir, name);

    /// <summary>The scanner output file that Setup reads.</summary>
    public string CapabilitiesFile => Path.Combine(DataDir, "scan", "capabilities.json");

    /// <summary>The configuration file of the service.</summary>
    public string IniFile => Path.Combine(DataDir, "config", "service.ini");

    /// <summary>The stored (protected) server key; only its existence is looked at.</summary>
    public string KeyFile => Path.Combine(DataDir, "config", "key.bin");

    /// <summary>The Setup log.</summary>
    public string LogFile => Path.Combine(DataDir, "logs", "install.log");
}

/// <summary>
/// Writes the lines of install.log: time, step, result word, error code. It never gets a key or a URL, because
/// callers pass only fixed step names and result words.
/// </summary>
public sealed class SetupLog
{
    private readonly SetupServices _services;
    private readonly SetupLayout _layout;
    private bool _enabled = true;

    /// <summary>Creates the log writer.</summary>
    public SetupLog(SetupServices services, SetupLayout layout)
    {
        _services = services;
        _layout = layout;
    }

    /// <summary>Stops all writing, used after the data folder was removed so the log does not bring it back.</summary>
    public void Suspend() => _enabled = false;

    /// <summary>Writes one line. A failed write is ignored: the log must never stop an install.</summary>
    /// <param name="step">Fixed step name.</param>
    /// <param name="result">Fixed result word, for example ok, failed, undone.</param>
    /// <param name="code">Error code, 0 when none.</param>
    public void Line(string step, string result, int code)
    {
        if (!_enabled) return;
        string line = $"{_services.System.NowUtcText()} {step} {result} {code}";
        _services.Files.AppendLine(_layout.LogFile, line);
    }
}
