// App.xaml.cs: start of Setup.exe: reads the command line and opens the window.
// Who uses it: Windows, when Setup.exe starts. `/uninstall` and `/settings` choose the mode; no argument is the install.
// An uninstall started from the installed folder first copies the whole folder to a temporary one and runs from there,
// because a program cannot delete the folder it runs from. `/tempcopy` marks that copy so the window can remove it.
using System.Windows;

namespace EcoWattSense.Setup;

/// <summary>The parsed command line.</summary>
/// <param name="Mode">Install, settings or uninstall.</param>
/// <param name="IsTempCopy">True when this is the temporary copy made for an uninstall.</param>
public sealed record SetupArguments(SetupMode Mode, bool IsTempCopy)
{
    /// <summary>Reads the arguments; case does not matter and an unknown argument is ignored.</summary>
    public static SetupArguments Parse(string[] args)
    {
        bool uninstall = args.Any(argument => argument.Equals("/uninstall", StringComparison.OrdinalIgnoreCase));
        bool settings = args.Any(argument => argument.Equals("/settings", StringComparison.OrdinalIgnoreCase));
        bool temp = args.Any(argument => argument.Equals("/tempcopy", StringComparison.OrdinalIgnoreCase));
        SetupMode mode = uninstall ? SetupMode.Uninstall : settings ? SetupMode.Settings : SetupMode.Install;
        return new SetupArguments(mode, temp);
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
        bool relaunch = arguments.Mode == SetupMode.Uninstall && !arguments.IsTempCopy && RunsFromProgramDir(packageDir, layout);
        if (relaunch) { RelaunchFromTemp(services, packageDir); return; }
        string? tempDir = arguments.IsTempCopy ? packageDir : null;
        new SetupWindow(arguments.Mode, services, layout, packageDir, tempDir).Show();
    }

    private static bool RunsFromProgramDir(string packageDir, SetupLayout layout) =>
        string.Equals(packageDir, layout.ProgramDir.TrimEnd('\\'), StringComparison.OrdinalIgnoreCase);

    /// <summary>Copies the folder to %TEMP% and starts the copy with /uninstall /tempcopy, then ends this process.</summary>
    private void RelaunchFromTemp(SetupServices services, string packageDir)
    {
        string copy = Path.Combine(Path.GetTempPath(), "EcoWattSenseSetup-" + Guid.NewGuid().ToString("N"));
        int code = services.Files.CopyDirectory(packageDir, copy);
        int started = code == 0 ? services.Processes.StartDetached(Path.Combine(copy, "Setup.exe"), "/uninstall /tempcopy") : code;
        if (started != 0) MessageBox.Show("Setup could not start the uninstall (error " + started + ").", "EcoWattSense Setup");
        Shutdown();
    }
}
