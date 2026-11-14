// StatusFolderWatcher.cs: raises an event when status.json changes, and only while the viewer window is visible (SPEC 9).
// Who uses it: the viewer window (MainWindow.Viewer.cs).
// Why SetActive creates the watcher late: the status folder may not exist yet (the service has never run). The watcher
// is then created on the next show, so there is no polling. The service writes a temporary file and renames it over
// status.json, so the Renamed event is handled as well.
#nullable enable
using System;
using System.IO;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Watches one status file and raises <see cref="Changed"/> on its events.</summary>
public sealed class StatusFolderWatcher : IDisposable
{
    private readonly string folder;
    private readonly string fileName;
    private FileSystemWatcher? watcher;

    /// <summary>Creates a watcher. It does nothing until <see cref="SetActive"/> is called.</summary>
    /// <param name="folder">The folder that holds the file.</param>
    /// <param name="fileName">The file name, for example status.json.</param>
    public StatusFolderWatcher(string folder, string fileName)
    {
        this.folder = folder;
        this.fileName = fileName;
    }

    /// <summary>Raised on a thread-pool thread when the file is created, changed or renamed into place.</summary>
    public event Action? Changed;

    /// <summary>Switches the events on or off.</summary>
    /// <param name="active">True while the window is visible.</param>
    /// <returns>True when the events are on. False when switched off, or when the folder does not exist yet.</returns>
    public bool SetActive(bool active)
    {
        if (!active) return Disable();
        if (watcher is null && !TryCreate()) return false;
        watcher!.EnableRaisingEvents = true;
        return true;
    }

    /// <summary>Switches the events off.</summary>
    /// <returns>Always false.</returns>
    private bool Disable()
    {
        if (watcher is not null) watcher.EnableRaisingEvents = false;
        return false;
    }

    /// <summary>Creates the watcher when the folder exists.</summary>
    /// <returns>True when the watcher was created.</returns>
    private bool TryCreate()
    {
        if (!Directory.Exists(folder)) return false;
        watcher = new FileSystemWatcher(folder, fileName)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.Size
        };
        watcher.Created += OnFileEvent;
        watcher.Changed += OnFileEvent;
        watcher.Renamed += OnRenamed;
        return true;
    }

    /// <summary>Handles a created or changed event.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnFileEvent(object sender, FileSystemEventArgs e) => Changed?.Invoke();

    /// <summary>Handles a rename into place.</summary>
    /// <param name="sender">Not used.</param>
    /// <param name="e">Not used.</param>
    private void OnRenamed(object sender, RenamedEventArgs e) => Changed?.Invoke();

    /// <summary>Releases the watcher.</summary>
    public void Dispose()
    {
        watcher?.Dispose();
        watcher = null;
    }
}
