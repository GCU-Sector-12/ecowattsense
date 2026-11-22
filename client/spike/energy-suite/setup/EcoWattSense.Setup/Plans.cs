// Plans.cs: the pure parts of Setup that build data and touch nothing: the service SID, the folder rights plan
// (SPEC 2.2 with the service SID instead of LocalService), the service.ini editor and the autostart task XML.
// Who uses it: the install engine, the settings engine and the tests. Pure code is easy to test without a machine.
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace EcoWattSense.Setup;

/// <summary>Computes the service SID NT SERVICE\name without asking Windows.</summary>
public static class ServiceSid
{
    /// <summary>
    /// The SID of a service is S-1-5-80 followed by the SHA-1 of the upper-case service name (UTF-16) as five
    /// 32-bit little-endian numbers. Computing it means the SID can be used in folder rights before the service
    /// exists, which a name lookup cannot do.
    /// </summary>
    /// <param name="serviceName">The service name.</param>
    /// <returns>The SID text.</returns>
    public static string Compute(string serviceName)
    {
        byte[] hash = SHA1.HashData(Encoding.Unicode.GetBytes(serviceName.ToUpperInvariant()));
        uint[] parts = Enumerable.Range(0, 5).Select(index => BitConverter.ToUInt32(hash, index * 4)).ToArray();
        return "S-1-5-80-" + string.Join("-", parts);
    }
}

/// <summary>One folder of the rights plan.</summary>
/// <param name="Path">The folder.</param>
/// <param name="Mode">Reset or Add.</param>
/// <param name="Entries">The entries to apply.</param>
/// <param name="EntriesWhenNew">Extra entries added first when the folder did not exist before (Client only).</param>
public sealed record FolderPlan(string Path, AclMode Mode, IReadOnlyList<AclEntry> Entries, IReadOnlyList<AclEntry> EntriesWhenNew);

/// <summary>Builds the folder rights of SPEC 2.2.</summary>
public static class AclPlan
{
    /// <summary>SYSTEM.</summary>
    public const string System = "S-1-5-18";

    /// <summary>Administrators.</summary>
    public const string Admins = "S-1-5-32-544";

    /// <summary>Users.</summary>
    public const string Users = "S-1-5-32-545";

    private static readonly (string Name, AccessRight? Service, AccessRight? Users)[] Table =
    [
        ("scan", AccessRight.Modify, AccessRight.Read),
        ("calibration", AccessRight.Read, AccessRight.Modify),
        ("history", AccessRight.Modify, AccessRight.Read),
        ("status", AccessRight.Modify, AccessRight.Read),
        ("queue", AccessRight.Modify, null),
        ("config", AccessRight.Read, null),
        ("logs", AccessRight.Modify, AccessRight.Read),
    ];

    /// <summary>
    /// Builds the plan in order: the data folder itself, Client, then the seven sub-folders. SYSTEM and
    /// Administrators always get full control. A sub-folder gets the service SID and Users as in the table;
    /// config gets only the service SID (read), Administrators and SYSTEM (decision of 14 Nov 2026).
    /// </summary>
    /// <param name="dataDir">The data folder.</param>
    /// <param name="serviceSid">The service SID text.</param>
    /// <returns>The folders in the order to apply them.</returns>
    public static IReadOnlyList<FolderPlan> Build(string dataDir, string serviceSid)
    {
        List<FolderPlan> plan = [Root(dataDir, serviceSid), Client(dataDir, serviceSid)];
        foreach ((string name, AccessRight? service, AccessRight? users) in Table)
        {
            plan.Add(SubFolder(dataDir, name, serviceSid, service, users));
        }
        return plan;
    }

    private static FolderPlan Root(string dataDir, string serviceSid)
    {
        AclEntry[] entries =
        [
            new(System, AccessRight.Full), new(Admins, AccessRight.Full),
            new(Users, AccessRight.Traverse), new(serviceSid, AccessRight.Traverse),
        ];
        return new FolderPlan(dataDir, AclMode.Reset, entries, []);
    }

    private static FolderPlan Client(string dataDir, string serviceSid) =>
        new(Path.Combine(dataDir, "Client"), AclMode.Add, [new(serviceSid, AccessRight.Modify)], [new(Users, AccessRight.Modify)]);

    private static FolderPlan SubFolder(string dataDir, string name, string serviceSid, AccessRight? service, AccessRight? users)
    {
        List<AclEntry> entries = [new(System, AccessRight.Full), new(Admins, AccessRight.Full)];
        if (service.HasValue) entries.Add(new AclEntry(serviceSid, service.Value));
        if (users.HasValue) entries.Add(new AclEntry(Users, users.Value));
        return new FolderPlan(Path.Combine(dataDir, name), AclMode.Reset, entries, []);
    }
}

/// <summary>Edits the text of service.ini one key at a time, keeping every other line.</summary>
public static class IniEditor
{
    /// <summary>Reads the value of a key, or an empty text when the key is missing.</summary>
    /// <param name="text">The whole file text.</param>
    /// <param name="key">The key.</param>
    public static string Get(string? text, string key)
    {
        Match match = Regex.Match(text ?? "", $@"^[ \t]*{Regex.Escape(key)}[ \t]*=[ \t]*(.*?)\r?$", RegexOptions.Multiline);
        return match.Success ? match.Groups[1].Value : "";
    }

    /// <summary>Sets a key: replaces its line, or appends one. Other lines stay. Lines end with a line feed.</summary>
    /// <param name="text">The whole file text, or null when there is no file yet.</param>
    /// <param name="key">The key.</param>
    /// <param name="value">The new value.</param>
    /// <returns>The new file text.</returns>
    public static string Set(string? text, string key, string value)
    {
        string[] lines = (text ?? "").Replace("\r\n", "\n").Split('\n', StringSplitOptions.RemoveEmptyEntries);
        string[] others = lines.Where(line => !IsKeyLine(line, key)).ToArray();
        return string.Concat(others.Select(line => line + "\n")) + key + "=" + value + "\n";
    }

    private static bool IsKeyLine(string line, string key)
    {
        int equals = line.IndexOf('=');
        return equals >= 0 && line[..equals].Trim() == key;
    }
}

/// <summary>Builds the XML of the client autostart task, with the design of Install-Autostart.ps1.</summary>
public static class TaskXml
{
    /// <summary>
    /// The task starts the client hidden one minute after log-on, for every user: a log-on trigger without a user
    /// and the principal group Users, interactive and not elevated (the client must run in the user's own session
    /// to see keyboard and mouse use). No battery limit, no time limit, up to three restarts one minute apart,
    /// no second copy at the same time. No key is passed, because task arguments are visible to every user.
    /// </summary>
    /// <param name="clientExe">Full path of the client.</param>
    /// <param name="workingDir">Folder of the client.</param>
    /// <param name="deviceType">laptop or desktop.</param>
    /// <returns>The task XML.</returns>
    public static string Build(string clientExe, string workingDir, string deviceType)
    {
        string arguments = $"--start-hidden --Reporting:DeviceType={deviceType}";
        string[] lines =
        [
            "<?xml version=\"1.0\" encoding=\"UTF-16\"?>",
            "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">",
            "<RegistrationInfo><Description>Starts the EcoWattSense client in the system tray when you log in.</Description></RegistrationInfo>",
            "<Triggers><LogonTrigger><Enabled>true</Enabled><Delay>PT1M</Delay></LogonTrigger></Triggers>",
            $"<Principals><Principal id=\"Users\"><GroupId>{AclPlan.Users}</GroupId><RunLevel>LeastPrivilege</RunLevel></Principal></Principals>",
            "<Settings>",
            "<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>",
            "<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>",
            "<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>",
            "<StartWhenAvailable>true</StartWhenAvailable>",
            "<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>",
            "<RestartOnFailure><Interval>PT1M</Interval><Count>3</Count></RestartOnFailure>",
            "</Settings>",
            $"<Actions Context=\"Users\"><Exec><Command>{Escape(clientExe)}</Command><Arguments>{Escape(arguments)}</Arguments><WorkingDirectory>{Escape(workingDir)}</WorkingDirectory></Exec></Actions>",
            "</Task>",
        ];
        return string.Join("\n", lines);
    }

    private static string Escape(string text) => System.Security.SecurityElement.Escape(text);
}

