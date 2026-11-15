// RenameDialog.xaml.cs: the small modal dialog "Rename this computer" (Settings page).
// Who uses it: OnRenameClick in MainWindow.Settings.cs. ShowDialog() gives true after the service pipe took the name.
// Save writes "EWS1 name=<base64>" to the service pipe (the existing ServicePipeSender, no UAC). The pipe write runs
// on a pool thread, so a missing or busy pipe never freezes the window.
#nullable enable
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using EcoWattSense.Client.Gui.Viewer;

namespace EcoWattSense.Client.Gui.Settings;

public partial class RenameDialog : Window
{
    private bool sending;

    /// <summary>Creates the dialog.</summary>
    /// <param name="currentName">The name shown now, or null when none; it is put into the box.</param>
    public RenameDialog(string? currentName)
    {
        InitializeComponent();
        NameBox.Text = currentName ?? "";
        NameBox.SelectAll();
        ShowCheck();
    }

    /// <summary>Checks the name again after every change of the box.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnNameChanged(object sender, TextChangedEventArgs e) => ShowCheck();

    /// <summary>Shows the reason line and sets the Save button from the check of the typed name.</summary>
    private void ShowCheck()
    {
        DeviceNameCheck check = DeviceNameRules.Check(NameBox.Text);
        ReasonText.Text = check.Reason;
        SaveButton.IsEnabled = check.Valid && !sending;
    }

    /// <summary>Sends the name to the service and closes the dialog, or says the service did not answer.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private async void OnSaveClick(object sender, RoutedEventArgs e)
    {
        string? message = DeviceNameRules.Message(DeviceNameRules.Check(NameBox.Text).Name);
        sending = true;
        ShowCheck();
        bool written = await Task.Run(() => new ServicePipeSender().Send(message));
        sending = false;
        ShowCheck();
        if (!written) ReasonText.Text = "The service did not answer.";
        if (written) DialogResult = true;
    }
}
