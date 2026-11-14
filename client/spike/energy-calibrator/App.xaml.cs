// App.xaml.cs: code behind of App.xaml.
// Who uses it: the WPF runtime. It reads the command line, sets the exit code (SPEC section 8) and handles errors that no other code caught.
// Command line: --data-dir <folder> replaces C:\ProgramData\EcoWattSense as the folder that holds "calibration\model.json" (for tests).
// Why the window stays open: a running test may have changed the brightness or turned the display off.
// The Stop button restores both, so the program must stay alive after an unexpected error.
using System.Windows;
using System.Windows.Threading;

namespace EnergyCalibrator;

/// <summary>The application object. Shows unexpected errors without closing the program.</summary>
public partial class App : Application
{
    /// <summary>The folder that holds "calibration\model.json" for this run of the program.</summary>
    internal static string DataDir { get; private set; } = ServiceModel.StandardDataDir();

    /// <summary>Reads the command line before the main window opens.</summary>
    /// <param name="e">The start-up arguments.</param>
    protected override void OnStartup(StartupEventArgs e)
    {
        DataDir = CommandLine.DataDir(e.Args);
        base.OnStartup(e);
    }

    /// <summary>Sets the exit code of the program from the last result (written, stopped, start not met, aborted, not writable).</summary>
    /// <param name="e">The exit details; only the exit code is set.</param>
    protected override void OnExit(ExitEventArgs e)
    {
        e.ApplicationExitCode = ExitCodes.Current;
        base.OnExit(e);
    }

    /// <summary>
    /// Shows a short message for an error nobody caught and marks it handled.
    /// Takes the sender and the error details. The message names only the error type, never data from the computer.
    /// </summary>
    private void OnUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        string text = $"Unexpected error ({e.Exception.GetType().Name}). Press Stop to restore the screen, then close the program.";
        MessageBox.Show(text, "Energy Calibrator", MessageBoxButton.OK, MessageBoxImage.Warning);
        e.Handled = true;
    }
}
