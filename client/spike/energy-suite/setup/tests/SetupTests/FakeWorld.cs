// FakeWorld.cs: a fake computer for the Setup tests. One object implements every operation interface of Setup and keeps
// its state in memory: folders, files, the service, the task, the programs that were run. Nothing touches the real machine.
// Who uses it: the check classes of SetupTests. `FailOn` names one operation that must fail once with error 5, so a test can
// make any step of the install fail.
using System.Text.RegularExpressions;
using EcoWattSense.Setup;

namespace SetupTests;

/// <summary>One call to a program, as the fake saw it.</summary>
/// <param name="Exe">The program path.</param>
/// <param name="Arguments">The command line text.</param>
/// <param name="Input">The standard input text, or null.</param>
public sealed record RunCall(string Exe, string Arguments, string? Input);

/// <summary>One folder-rights call.</summary>
/// <param name="Path">The folder.</param>
/// <param name="Mode">Reset or Add.</param>
/// <param name="Entries">The entries.</param>
public sealed record AclCall(string Path, AclMode Mode, IReadOnlyList<AclEntry> Entries);

/// <summary>The fake computer.</summary>
public sealed class FakeWorld : IFileOps, IAclOps, IServiceOps, ITaskOps, IProcessOps, ISystemInfo
{
    /// <summary>The scan file the fake scanner writes.</summary>
    public const string ScanJson = """
        {"schema":1,"context":"admin","sources":[
          {"id":"emi","present":true,"readable":true,"live":true,"error":0},
          {"id":"battery","present":true,"readable":true,"live":null,"error":0,"unit":"mWh"},
          {"id":"gpu","present":false}],
         "level":"L2","calibration":"needed","device_type":"laptop"}
        """;

    /// <summary>Folders that exist.</summary>
    public HashSet<string> Directories { get; } = [];

    /// <summary>Files and their text.</summary>
    public Dictionary<string, string> Files { get; } = [];

    /// <summary>Every operation in the order it happened, such as "copydir" or "service.start".</summary>
    public List<string> Calls { get; } = [];

    /// <summary>Every program run.</summary>
    public List<RunCall> Runs { get; } = [];

    /// <summary>Every folder-rights call.</summary>
    public List<AclCall> Acls { get; } = [];

    /// <summary>Task names and XML that exist.</summary>
    public Dictionary<string, string> Tasks { get; } = [];

    /// <summary>True when the service is registered.</summary>
    public bool ServiceExists { get; set; }

    /// <summary>True when the service is running.</summary>
    public bool ServiceRunning { get; set; }

    /// <summary>The last registration, or null.</summary>
    public ServiceSpec? Registered { get; private set; }

    /// <summary>How many times the service was registered.</summary>
    public int RegisterCount { get; private set; }

    /// <summary>How many files the fake package folder holds; the fake copy reports each one.</summary>
    public int CopyFileCount { get; set; } = 3;

    /// <summary>Result of starting the client at Finish.</summary>
    public int ClientStartExit { get; set; }

    /// <summary>Exit code the fake calibrator returns.</summary>
    public int CalibratorExit { get; set; } = 7;

    /// <summary>How many client or calibrator processes the fake computer has running; CloseClients closes them all.</summary>
    public int ClientsRunning { get; set; }

    /// <summary>The operation that must fail with error 5: stop, close_client, copydir, acl, configure, register, start, task, scan.</summary>
    public string FailOn { get; set; } = "";

    /// <summary>Result of the Windows 11 x64 check.</summary>
    public bool Windows11 { get; set; } = true;

    /// <summary>Result of the administrator check.</summary>
    public bool Administrator { get; set; } = true;

    /// <summary>The services record for the engines.</summary>
    public SetupServices Services => new(this, this, this, this, this, this);

    private int Result(string operation)
    {
        Calls.Add(operation);
        bool fail = FailOn == operation;
        FailOn = fail ? "" : FailOn;
        return fail ? 5 : 0;
    }

    /// <inheritdoc />
    public bool DirectoryExists(string path) => Directories.Contains(path);

    /// <inheritdoc />
    public bool FileExists(string path) => Files.ContainsKey(path);

    /// <inheritdoc />
    public int CreateDirectory(string path)
    {
        Directories.Add(path);
        return 0;
    }

    /// <inheritdoc />
    public int CountFiles(string path) => CopyFileCount;

    /// <inheritdoc />
    public int CopyDirectory(string source, string target, IProgress<int>? copied = null)
    {
        int code = Result("copydir");
        if (code != 0) return code;
        Directories.Add(target);
        for (int count = 1; count <= CopyFileCount; count++) copied?.Report(count);
        return 0;
    }

    /// <inheritdoc />
    public int DeleteDirectory(string path)
    {
        Calls.Add("deletedir " + path);
        Directories.RemoveWhere(folder => IsInside(folder, path));
        foreach (string file in Files.Keys.Where(name => IsInside(name, path)).ToList()) Files.Remove(file);
        return 0;
    }

    private static bool IsInside(string candidate, string folder) =>
        candidate == folder || candidate.StartsWith(folder + "\\", StringComparison.OrdinalIgnoreCase);

    /// <inheritdoc />
    public string? ReadText(string path) => Files.GetValueOrDefault(path);

    /// <inheritdoc />
    public int WriteText(string path, string text)
    {
        Files[path] = text;
        return 0;
    }

    /// <inheritdoc />
    public int DeleteFile(string path)
    {
        Calls.Add("deletefile " + path);
        Files.Remove(path);
        return 0;
    }

    /// <inheritdoc />
    public int AppendLine(string path, string line)
    {
        Files[path] = Files.GetValueOrDefault(path, "") + line + "\n";
        Directories.Add(Path.GetDirectoryName(path)!);
        return 0;
    }

    /// <inheritdoc />
    public int Apply(string path, AclMode mode, IReadOnlyList<AclEntry> entries)
    {
        Acls.Add(new AclCall(path, mode, entries));
        return Result("acl");
    }

    /// <inheritdoc />
    public bool Exists() => ServiceExists;

    /// <inheritdoc />
    public bool IsRunning() => ServiceRunning;

    /// <inheritdoc />
    public int Stop()
    {
        int code = Result("stop");
        if (code == 0) ServiceRunning = false;
        return code;
    }

    /// <inheritdoc />
    public int Start()
    {
        int code = Result("start");
        if (code == 0) ServiceRunning = true;
        return code;
    }

    /// <inheritdoc />
    public int Register(ServiceSpec spec)
    {
        int code = Result("register");
        if (code != 0) return code;
        ServiceExists = true;
        Registered = spec;
        RegisterCount++;
        return 0;
    }

    /// <inheritdoc />
    public int Delete()
    {
        Calls.Add("service.delete");
        ServiceExists = false;
        ServiceRunning = false;
        return 0;
    }

    /// <inheritdoc />
    public int Register(string name, string xml)
    {
        int code = Result("task");
        if (code == 0) Tasks[name] = xml;
        return code;
    }

    /// <inheritdoc />
    public int Delete(string name)
    {
        Calls.Add("task.delete");
        Tasks.Remove(name);
        return 0;
    }

    /// <inheritdoc />
    public bool Exists(string name) => Tasks.ContainsKey(name);

    /// <inheritdoc />
    public int Run(string exePath, string arguments, string? standardInput)
    {
        Runs.Add(new RunCall(exePath, arguments, standardInput));
        return exePath.EndsWith("ews-scan.exe") ? RunScan(arguments) : RunService(standardInput);
    }

    private int RunScan(string arguments)
    {
        int code = Result("scan");
        string folder = Regex.Match(arguments, "--out \"([^\"]+)\"").Groups[1].Value;
        if (code == 0) Files[Path.Combine(folder, "capabilities.json")] = ScanJson;
        return code;
    }

    /// <summary>The fake `--configure`: a `key=` line stores a key file, a `url=` line writes server_url.</summary>
    private int RunService(string? input)
    {
        int code = Result("configure");
        if (code != 0) return 1;
        string text = input ?? "";
        if (text.Contains("key=")) Files[@"P:\Data\config\key.bin"] = "protected";
        if (text.Contains("url=")) Files[@"P:\Data\config\service.ini"] = IniEditor.Set(Files.GetValueOrDefault(@"P:\Data\config\service.ini"), "server_url", "set");
        return 0;
    }

    /// <inheritdoc />
    public int RunAsLoggedOnUser(string exePath)
    {
        Calls.Add("calibrator " + exePath);
        return CalibratorExit;
    }

    /// <inheritdoc />
    public int StartAsLoggedOnUser(string exePath)
    {
        Calls.Add("client " + exePath);
        return ClientStartExit;
    }

    /// <inheritdoc />
    public int CloseClients(out int closed)
    {
        int code = Result("close_client");
        closed = code == 0 ? ClientsRunning : 0;
        ClientsRunning = code == 0 ? 0 : ClientsRunning;
        return code;
    }

    /// <inheritdoc />
    public int StartDetached(string exePath, string arguments)
    {
        Calls.Add("detached " + exePath);
        return 0;
    }

    /// <inheritdoc />
    public bool IsWindows11X64() => Windows11;

    /// <inheritdoc />
    public bool IsAdministrator() => Administrator;

    /// <inheritdoc />
    public string NowUtcText() => "2026-11-14T10:00:00Z";
}

