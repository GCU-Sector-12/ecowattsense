// InstallProgress.cs: what the install reports to the window while it runs (SPEC section 10).
// Who uses it: StepRunner and InstallEngine report one InstallProgress for each change of a step; the window turns each
// report into a mark in the step list and a move of the progress bar. The window passes a Progress<T> made on the UI
// thread, so the reports arrive on the UI thread; the tests pass a collector. Nothing here knows WPF.
namespace EcoWattSense.Setup;

/// <summary>The state of one install step.</summary>
public enum StepState
{
    /// <summary>Not started yet.</summary>
    Waiting,
    /// <summary>Running now.</summary>
    Working,
    /// <summary>Finished well.</summary>
    Done,
    /// <summary>Stopped with a problem.</summary>
    Failed,
}

/// <summary>One report about one step.</summary>
/// <param name="Name">The fixed step name, as in install.log.</param>
/// <param name="State">The new state of the step.</param>
/// <param name="Text">The line to show: the plain title, the title with a file count, or the failure message.</param>
/// <param name="Done">Files copied so far, for the copy step; 0 otherwise.</param>
/// <param name="Total">Files to copy, for the copy step; 0 otherwise.</param>
public sealed record InstallProgress(string Name, StepState State, string Text, int Done = 0, int Total = 0);

/// <summary>The steps of the install in plain words, in the order they run.</summary>
public static class InstallTitles
{
    /// <summary>The step names in running order, including the last one that sends the computer name.</summary>
    public static readonly string[] Order =
    [
        "stop_old_service", "close_client", "copy_programs", "data_folders", "configure",
        "register_service", "start_service", "autostart_task", "device_name",
    ];

    /// <summary>The step names of the upgrade in running order (no configuration, no computer name).</summary>
    public static readonly string[] UpgradeOrder =
    [
        "close_client", "stop_old_service", "copy_programs", "data_folders", "start_service", "autostart_task",
    ];

    /// <summary>The step list of a mode.</summary>
    /// <param name="upgrade">True for the upgrade list.</param>
    public static string[] OrderFor(bool upgrade) => upgrade ? UpgradeOrder : Order;

    private static readonly Dictionary<string, string> Titles = new()
    {
        ["stop_old_service"] = "Stopping the old service",
        ["close_client"] = "Closing the client",
        ["copy_programs"] = "Copying programs",
        ["data_folders"] = "Creating data folders",
        ["configure"] = "Writing the configuration",
        ["register_service"] = "Registering the service",
        ["start_service"] = "Starting the service",
        ["autostart_task"] = "Client autostart",
        ["device_name"] = "Sending the computer name",
        ["stop_service"] = "Stopping the service",
        ["delete_service"] = "Deleting the service",
        ["delete_task"] = "Deleting the autostart task",
        ["delete_programs"] = "Deleting the programs",
        ["remove_data"] = "Removing the data",
    };

    /// <summary>The plain title of a step; the name itself when the step has no title.</summary>
    /// <param name="name">The fixed step name.</param>
    public static string For(string name) => Titles.GetValueOrDefault(name, name);
}
