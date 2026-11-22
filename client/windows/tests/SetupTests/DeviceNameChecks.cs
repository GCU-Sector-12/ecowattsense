// DeviceNameChecks.cs: tests of the "Computer name" field logic (DeviceNameSync, DeviceNameRules) with a fake HTTP handler.
// Nothing leaves the computer: the handler answers from memory and FakeWorld holds the files. Run by Program.Main.
using System.Net;
using System.Net.Http;
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>The checks of the computer name.</summary>
public static class DeviceNameChecks
{
    private const string Id = "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12";
    private const string Good = "https://server.example.org";
    private const string Name = "Lab 1 PC 03";
    private static readonly string NameFile = Kit.Layout.DeviceNameFile;

    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        RuleChecks();
        SendChecks();
        FileChecks();
        FailureChecks();
        InstallChecksWithName();
        LogChecks();
    }

    private static FakeWorld WorldWithId()
    {
        FakeWorld world = new();
        world.Files[Kit.Layout.DeviceIdFile] = Id + "\n";
        return world;
    }

    private static DeviceNameSync Sync(FakeWorld world, FakeHandler handler) =>
        new(world.Services, Kit.Layout, handler, () => "", new SetupLog(world.Services, Kit.Layout));

    private static void RuleChecks()
    {
        Check(DeviceNameRules.Clean("  Lab 1  ") == "Lab 1", "device name: spaces at both ends are removed");
        Check(DeviceNameRules.IsValid(new string('a', 60)) && !DeviceNameRules.IsValid(new string('a', 61)), "device name: 60 characters are the limit");
        Check(DeviceNameRules.IsValid("  " + new string('a', 60) + "  "), "device name: the limit counts the cleaned name");
        Check(!DeviceNameRules.IsValid("a\u0001b") && !DeviceNameRules.IsValid("a\tb") && !DeviceNameRules.IsValid("a\nb"), "device name: control characters are refused");
        Check(DeviceNameRules.IsValid(""), "device name: an empty name is allowed");
        Check(DeviceNameRules.FromFile("Lab 1\r\nsecond line") == "Lab 1" && DeviceNameRules.FromFile(null) == "", "device name: the file gives its first line");
    }

    private static void SendChecks()
    {
        FakeWorld world = WorldWithId();
        FakeHandler handler = new();
        StepOutcome outcome = Sync(world, handler).Apply("  " + Name + " ", Good, "key-1", "laptop");
        Check(outcome.Ok && outcome.Message == "", "device name: a good send gives no message");
        Check(handler.Last?.Method == HttpMethod.Put && handler.Last?.RequestUri?.AbsolutePath == "/api/devices/" + Id, "device name: PUT to /api/devices/<id>");
        Check(handler.LastKey == "key-1", "device name: the key goes in X-EWS-Key");
        Check(handler.LastBody == "{\"device_name\":\"" + Name + "\",\"os\":\"windows\",\"device_type\":\"laptop\"}", "device name: the body is exact");
        Sync(world, handler).Apply("", Good, "key-1", "desktop");
        Check(handler.LastBody == "{\"device_name\":null,\"os\":\"windows\",\"device_type\":\"desktop\"}", "device name: an empty name is sent as null");
    }

    private static void FileChecks()
    {
        FakeWorld world = WorldWithId();
        DeviceNameSync sync = Sync(world, new FakeHandler());
        sync.Apply(" Lab 1 ", "", "", "laptop");
        Check(world.Files[NameFile] == "Lab 1\n", "device name: the file holds one cleaned line");
        sync.Apply("", "", "", "laptop");
        Check(!world.Files.ContainsKey(NameFile), "device name: an empty name deletes the file");
        FakeHandler handler = new();
        Sync(world, handler).Apply("Lab", "", "", "laptop");
        Check(handler.Last == null, "device name: no address and no key means no request");
        Sync(world, handler).Apply("Lab", Good, "", "laptop");
        Check(handler.Last == null, "device name: no key means no request");
        world.Files[Kit.Layout.IniFile] = "server_url=" + Good + "\n";
        Sync(world, handler).Apply("Lab", "", "typed", "laptop");
        Check(handler.Last?.RequestUri?.Host == "server.example.org", "device name: the stored address is used when none is typed");
    }

    private static void FailureChecks()
    {
        FakeWorld world = WorldWithId();
        StepOutcome refused = Sync(world, new FakeHandler { Status = HttpStatusCode.Unauthorized }).Apply("Lab", Good, "k", "laptop");
        Check(!refused.Ok && refused.Message == "The computer name could not be sent: the server refused the key.", "device name: refused gives one line");
        FakeHandler down = new() { Throw = new HttpRequestException(HttpRequestError.ConnectionError, "x") };
        StepOutcome unreachable = Sync(world, down).Apply("Lab", Good, "k", "laptop");
        Check(!unreachable.Ok && unreachable.Message == "The computer name could not be sent: server not reachable.", "device name: network failure gives one line");
        StepOutcome noId = Sync(new FakeWorld(), new FakeHandler()).Apply("Lab", Good, "k", "laptop");
        Check(noId.Message == "The computer name could not be sent: this computer has no id yet.", "device name: no id gives one line");
        Check(world.Files[NameFile] == "Lab\n", "device name: the file stays saved when the send fails");
        FakeHandler handler = new();
        StepOutcome invalid = Sync(world, handler).Apply("a\u0001", Good, "k", "laptop");
        Check(!invalid.Ok && handler.Last == null && world.Files[NameFile] == "Lab\n", "device name: an invalid name changes nothing and sends nothing");
        StepOutcome plain = Sync(world, new FakeHandler()).Apply("Lab", "http://server.example.org", "k", "laptop");
        Check(plain.Message.Contains("https"), "device name: http to a remote host is refused");
    }

    private static void InstallChecksWithName()
    {
        FakeWorld world = new();
        world.Files[Kit.Layout.DeviceIdFile] = Id;
        FakeHandler handler = new() { Status = HttpStatusCode.InternalServerError };
        SetupLog log = new(world.Services, Kit.Layout);
        InstallEngine engine = new(world.Services, Kit.Layout, log, Sync(world, handler));
        engine.Prepare(Kit.Package);
        StepOutcome outcome = engine.Install(new InstallRequest(Kit.Package, Good, Kit.Key, true, Name));
        Check(outcome.Ok && outcome.Note == "The computer name could not be sent: server answered 500.", "device name: a failed send does not fail the install");
        Check(world.ServiceRunning && world.Files[NameFile] == Name + "\n", "device name: the service runs and the name is saved");
        Check(handler.LastKey == Kit.Key, "device name: the install sends the typed key");
        MaintenanceEngine maintenance = new(world.Services, Kit.Layout, log, Sync(world, new FakeHandler()));
        StepOutcome saved = maintenance.SaveSettings("", "", true, "desktop", true, "New name");
        Check(saved.Ok && saved.Note == "" && world.Files[NameFile] == "New name\n", "device name: settings save writes the name");
    }

    private static void LogChecks()
    {
        FakeWorld world = WorldWithId();
        SetupLog log = new(world.Services, Kit.Layout);
        DeviceNameSync sync = new(world.Services, Kit.Layout, new FakeHandler(), () => "", log);
        sync.Apply(Name, "https://secret.example.org", "SECRET-KEY-12345", "laptop");
        string text = Kit.LogText(world);
        Check(text.Contains("device_name sent 0"), "device name: the log has the result word");
        Check(!text.Contains(Name) && !text.Contains("secret.example.org") && !text.Contains("SECRET-KEY"), "device name: no name, address or key in the log");
    }
}
