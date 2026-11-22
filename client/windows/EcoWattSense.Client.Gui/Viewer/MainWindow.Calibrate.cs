// MainWindow.Calibrate.cs: the Calibrate button of the Calibrated card (viewer mode).
// Who uses it: StartViewer (start), RefreshViewer (update) and OnViewerClosed (stop) in MainWindow.Viewer.cs.
// The button is enabled only on battery with a charge of 30 % or more, with the calibrator found and not running
// (CalibrateButtonState). Power changes arrive from SystemEvents.PowerModeChanged and the end of the calibrator from
// Process.Exited (CalibratorRunner); there is no timer. The calibrator starts as the same user, without elevation.
#nullable enable
using System.Windows;
using System.Windows.Threading;
using EcoWattSense.Client.Gui.Settings;
using EcoWattSense.Client.Gui.Viewer;
using Microsoft.Win32;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow
{
    private CalibratorRunner? calibrator;

    /// <summary>Starts watching the power state and the calibrator, and sets the button once.</summary>
    private void StartCalibrate()
    {
        calibrator = new CalibratorRunner();
        calibrator.Ended += QueueCalibrateUpdate;
        SystemEvents.PowerModeChanged += OnPowerModeChanged;
        UpdateCalibrateButton();
    }

    /// <summary>Stops watching. The static SystemEvents event must be removed, or it keeps the window alive.</summary>
    private void StopCalibrate()
    {
        SystemEvents.PowerModeChanged -= OnPowerModeChanged;
        calibrator?.Dispose();
    }

    /// <summary>Queues a button update when the power source or the charge changes. Runs on a system thread.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnPowerModeChanged(object sender, PowerModeChangedEventArgs e) => QueueCalibrateUpdate();

    /// <summary>Queues a button update on the window thread. May be called from any thread.</summary>
    private void QueueCalibrateUpdate()
        => Dispatcher.BeginInvoke(new Action(UpdateCalibrateButton), DispatcherPriority.Background);

    /// <summary>Reads the facts and sets the button and the line that says why it is disabled.</summary>
    private void UpdateCalibrateButton()
    {
        bool running = calibrator?.Refresh() ?? false;
        bool found = ProgramLocator.FindCalibrator() is not null;
        CalibrateState state = CalibrateButtonState.Decide(ViewerSources.ReadOnBattery(), ViewerSources.ReadChargePercent(), found, running);
        CalibrateButton.IsEnabled = state.Enabled;
        CalibrateNote.Text = state.Reason;
        CalibrateNote.Visibility = state.Enabled ? Visibility.Collapsed : Visibility.Visible;
    }

    /// <summary>Starts the calibrator.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnCalibrateClick(object sender, RoutedEventArgs e)
    {
        string? path = ProgramLocator.FindCalibrator();
        bool started = path is not null && calibrator is not null && calibrator.Start(path);
        UpdateCalibrateButton();
        if (path is not null && !started) CalibrateNote.Text = "Calibrator could not start.";
        if (path is not null && !started) CalibrateNote.Visibility = Visibility.Visible;
    }
}
