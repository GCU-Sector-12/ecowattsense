// App.xaml.cs: code behind of App.xaml.
// Who uses it: the WPF runtime. It only handles errors that no other code caught.
// Why the window stays open: a running test may have changed the brightness or turned the display off.
// The Stop button restores both, so the program must stay alive after an unexpected error.
using System.Windows;
using System.Windows.Threading;

namespace EnergyCalibrator;

/// <summary>The application object. Shows unexpected errors without closing the program.</summary>
public partial class App : Application
{
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
