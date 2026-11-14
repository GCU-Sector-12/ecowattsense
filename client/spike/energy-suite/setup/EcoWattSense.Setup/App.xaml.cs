// App.xaml.cs: start of Setup.exe: reads the command line and opens the window.
// Who uses it: Windows, when Setup.exe starts. `/uninstall`, `/settings` and `/upgrade` choose the mode; no argument is the install.
// An uninstall started from the installed folder first copies the whole folder to a temporary one and runs from there,
// because a program cannot delete the folder it runs from. `/tempcopy` marks that copy so the window can remove it.
using System.Windows;

namespace EcoWattSense.Setup;

/// <summary>The parsed command line.</summary>
/// <param name="Mode">Install, settings, uninstall or upgrade.</param>
/// <param name="IsTempCopy">True when this is the temporary copy made for an uninstall.</param>
/// <param name="ServerUrl">The cleaned /server=<url> value, or empty (see ServerUrlRules).</param>
public sealed record SetupArguments(SetupMode Mode, bool IsTempCopy, string ServerUrl = "")
{
    /// <summary>The text after "/server=" of one argument, or empty when it is another argument.</summary>
    private static string ServerValue(string argument) =>
        argument.StartsWith("/server=", StringComparison.OrdinalIgnoreCase) ? argument["/server=".Length..] : "";

    /// <summary>Reads the arguments; case does not matter and an unknown argument is ignored.</summary>
    public static SetupArguments Parse(string[] args)
    {
        bool uninstall = args.Any(argument => argument.Equals("/uninstall", StringComparison.OrdinalIgnoreCase));
        bool upgrade = args.Any(argument => argument.Equals("/upgrade", StringComparison.OrdinalIgnoreCase));
        bool settings = args.Any(argument => argument.Equals("/settings", StringComparison.OrdinalIgnoreCase));
        bool temp = args.Any(argument => argument.Equals("/tempcopy", StringComparison.OrdinalIgnoreCase));
        SetupMode mode = uninstall ? SetupMode.Uninstall : settings ? SetupMode.Settings : upgrade ? SetupMode.Upgrade : SetupMode.Install;
        string server = args.Select(ServerValue).FirstOrDefault(text => text.Length > 0) ?? "";
        return new SetupArguments(mode, temp, ServerUrlRules.Clean(server));
    }
}

/// <summary>Starts the uninstall from a temporary copy of the Setup folder.</summary>
public static class UninstallRelaunch
{
    /// <summary>
    /// Copies the folder to a new folder under tempRoot and starts the copy with /uninstall /tempcopy. The new process
    /// gets the copy as its working directory: it would otherwise inherit the program folder and keep it locked.
    /// </summary>
    /// <param name="services">The operations.</param>
    /// <param name="packageDir">The folder Setup runs from.</param>
    /// <param name="tempRoot">The temporary folder that holds the copy.</param>
    /// <returns>0 or an error code.</returns>
    public static int Start(SetupServices services, string packageDir, string tempRoot)
    {
        string copy = Path.Combine(tempRoot, "EcoWattSenseSetup-" + Guid.NewGuid().ToString("N"));
        int code = services.Files.CopyDirectory(packageDir, copy);
        return code == 0 ? services.Processes.StartDetached(Path.Combine(copy, "Setup.exe"), "/uninstall /tempcopy", copy) : code;
    }
}

/// <summary>The application.</summary>
public partial class App : Application
{
    /// <summary>Opens the window for the chosen mode, or relaunches from a temporary copy for an uninstall.</summary>
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        SetupArguments arguments = SetupArguments.Parse(e.Args);
        SetupServices services = new(new RealFileOps(), new RealAclOps(), new RealServiceOps(), new RealTaskOps(), new RealProcessOps(), new RealSystemInfo());
        SetupLayout layout = SetupLayout.Standard;
        string packageDir = AppContext.BaseDirectory.TrimEnd('\\');
        SetupMode mode = InstallEngine.ChooseMode(arguments.Mode, services, layout);
        bool relaunch = arguments.Mode == SetupMode.Uninstall && !arguments.IsTempCopy && RunsFromProgramDir(packageDir, layout);
        if (relaunch) { RelaunchFromTemp(services, packageDir); return; }
        string? tempDir = arguments.IsTempCopy ? packageDir : null;
        new SetupWindow(mode, services, layout, packageDir, tempDir, arguments.ServerUrl).Show();
    }

    private static bool RunsFromProgramDir(string packageDir, SetupLayout layout) =>
        string.Equals(packageDir, layout.ProgramDir.TrimEnd('\\'), StringComparison.OrdinalIgnoreCase);

    /// <summary>Copies the folder to %TEMP% and starts the copy with /uninstall /tempcopy, then ends this process.</summary>
    private void RelaunchFromTemp(SetupServices services, string packageDir)
    {
        int started = UninstallRelaunch.Start(services, packageDir, Path.GetTempPath());
        if (started != 0) MessageBox.Show("Setup could not start the uninstall (error " + started + ").", "EcoWattSense Setup");
        Shutdown();
    }
}
