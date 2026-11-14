// MainWindow.SendNow.cs: the waiting-reports line and the "Send now" button of the Last report card (viewer mode).
// Who uses it: RefreshViewer in MainWindow.Viewer.cs calls ShowSendNow on every status.json change.
// A click writes "EWS1 send" to the service pipe (the existing ServicePipeSender). The line then reads "Sending..."
// until the next status.json change. The button stays disabled for 30 seconds after a click, because the service
// accepts one request per 30 seconds; one delayed call (not a timer loop) enables it again.
#nullable enable
using System.Threading.Tasks;
using System.Windows;
using EcoWattSense.Client.Gui.Viewer;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow
{
    private int? sendQueued;
    private bool sendServerSet;
    private bool sendInProgress;
    private bool sendFailed;
    private DateTime sendLockedUntilUtc = DateTime.MinValue;

    /// <summary>Takes the new status: a status change ends "Sending..." and the failure line.</summary>
    /// <param name="status">The checked status.json content.</param>
    private void ShowSendNow(StatusInfo status)
    {
        sendQueued = status.Queued;
        sendServerSet = status.ServerSet;
        sendInProgress = false;
        sendFailed = false;
        UpdateSendNow();
    }

    /// <summary>Sets the line and the button from the facts kept in the fields.</summary>
    private void UpdateSendNow()
    {
        string note = SendNowState.Note(sendQueued, sendInProgress, sendFailed);
        SendNowNote.Text = note;
        SendNowNote.Visibility = note.Length > 0 ? Visibility.Visible : Visibility.Collapsed;
        SendNowButton.IsEnabled = SendNowState.ButtonEnabled(sendQueued, sendServerSet, DateTime.UtcNow, sendLockedUntilUtc);
    }

    /// <summary>Writes the send message to the service pipe, locks the button for 30 seconds and shows the result.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private async void OnSendNowClick(object sender, RoutedEventArgs e)
    {
        sendLockedUntilUtc = DateTime.UtcNow.AddSeconds(SendNowState.LockSeconds);
        SendNowButton.IsEnabled = false;
        bool written = await Task.Run(WriteSendNow);
        sendInProgress = written;
        sendFailed = !written;
        UpdateSendNow();
        await Task.Delay(TimeSpan.FromSeconds(SendNowState.LockSeconds));
        UpdateSendNow();
    }

    /// <summary>Writes the message on a pool thread, so a missing or busy pipe never freezes the window.</summary>
    /// <returns>True when the service pipe took the message.</returns>
    private static bool WriteSendNow() => new ServicePipeSender().Send(SendNowState.Message);
}
