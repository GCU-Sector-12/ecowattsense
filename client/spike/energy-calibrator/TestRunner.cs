// TestRunner.cs: runs the seven phases one after the other, applies the brightness and the load of each phase, stamps the samples
// with the phase, watches the safety limits and always restores the laptop at the end.
// Who uses it: MainWindow creates one TestRunner for each run and awaits RunAsync.
// How it works: it lives on the UI thread. Samples arrive from the SamplingLoop. A phase lasts as long as a wait on a
// cancellation token; a stop request (Stop button, charger, low battery, input while the display is off, error) cancels that wait.
// Whatever happens, RestoreAsync puts the processor power settings back (the session that changed them is given by MainWindow),
// puts the brightness back, stops the load, closes the graphics window and turns the display on.
namespace EnergyCalibrator;

/// <summary>Why a run stopped before the last phase ended.</summary>
internal enum AbortReason
{
    /// <summary>The run was not stopped.</summary>
    None,
    /// <summary>The user pressed Stop or closed the window.</summary>
    UserStop,
    /// <summary>The charger was plugged in.</summary>
    Charger,
    /// <summary>The battery fell below the limit.</summary>
    BatteryLow,
    /// <summary>The mouse or keyboard was used while the display was off.</summary>
    UserInput,
    /// <summary>An error stopped the run.</summary>
    Failure,
    /// <summary>The display did not report "off" within the time limit after the command.</summary>
    DisplayStayedOn,
    /// <summary>The display reported "on" or "dimmed" while the Screen off phase ran.</summary>
    DisplayWoke
}

/// <summary>Tells the window that a phase started or ended.</summary>
/// <param name="index">Index of the phase.</param>
/// <param name="status">Running when it started, the final status when it ended.</param>
internal sealed class PhaseEventArgs(int index, PhaseStatus status) : EventArgs
{
    /// <summary>Index of the phase.</summary>
    public int Index { get; } = index;

    /// <summary>The status of the phase.</summary>
    public PhaseStatus Status { get; } = status;
}

/// <summary>Everything a run produced.</summary>
/// <param name="StartedAt">Wall-clock time when the run started.</param>
/// <param name="Samples">Every sample taken during the run.</param>
/// <param name="Statuses">The final status of every phase.</param>
/// <param name="Notes">A note for every phase (empty when none), for example a brightness that could not be set.</param>
/// <param name="Reason">Why the run stopped early, or None.</param>
/// <param name="ReasonText">The reason as a sentence.</param>
/// <param name="RestoreProblems">Things that could not be put back at the end.</param>
internal sealed record TestOutcome(
    DateTime StartedAt,
    IReadOnlyList<Sample> Samples,
    IReadOnlyList<PhaseStatus> Statuses,
    IReadOnlyList<string> Notes,
    AbortReason Reason,
    string ReasonText,
    IReadOnlyList<string> RestoreProblems);

/// <summary>Runs one test from the first phase to the last, or until it is stopped.</summary>
internal sealed class TestRunner
{
    private readonly SamplingLoop sampling;
    private readonly BrightnessControl brightness = new();
    private readonly CpuLoad cpuLoad = new();
    private readonly CancellationTokenSource runCancel = new();
    private readonly List<Sample> samples = new();
    private readonly PhaseStatus[] statuses = new PhaseStatus[PhaseCatalogue.All.Count];
    private readonly bool[] closed = new bool[PhaseCatalogue.All.Count];
    private readonly string[] notes = Enumerable.Repeat("", PhaseCatalogue.All.Count).ToArray();
    private readonly List<string> restoreProblems = new();
    private GraphicsWindow? graphics;
    private PhaseDefinition? activePhase;
    private DateTime startedAt = DateTime.Now;
    private int? originalBrightness;
    private int nextPhaseIndex;
    private int currentIndex = -1;
    private double phaseStartSeconds;
    private bool displayIsOff;
    private AbortReason abortReason = AbortReason.None;
    private string failureText = "";
    private readonly IntPtr windowHandle;
    private readonly PowerSettingsSession power;

    /// <summary>Creates a runner.</summary>
    /// <param name="sampling">The loop that delivers a sample every 2 seconds.</param>
    /// <param name="windowHandle">Handle of the main window; the display-off command is sent to it.</param>
    /// <param name="power">The session that changed the power settings for this run; the runner restores them in its clean-up.</param>
    public TestRunner(SamplingLoop sampling, IntPtr windowHandle, PowerSettingsSession power)
    {
        this.sampling = sampling;
        this.windowHandle = windowHandle;
        this.power = power;
    }

    /// <summary>Raised when the clock of a phase starts.</summary>
    public event EventHandler<PhaseEventArgs>? PhaseStarted;

    /// <summary>Raised when a phase ends, with its final status.</summary>
    public event EventHandler<PhaseEventArgs>? PhaseEnded;

    /// <summary>Raised after a sample was stamped with its phase and kept. The handler must not throw.</summary>
    public event EventHandler<Sample>? SampleAccepted;

    /// <summary>Gives what the run has produced so far. It can be called while the run is going, for example when a phase ends.</summary>
    /// <returns>The samples, statuses and notes up to now.</returns>
    public TestOutcome Snapshot() => BuildOutcome();

    /// <summary>Runs the test. Call it on the UI thread. It never throws; an error ends the run with the reason Failure.</summary>
    /// <returns>The samples, statuses and the reason the run ended.</returns>
    public async Task<TestOutcome> RunAsync()
    {
        await RunGuardedAsync();
        await RestoreAsync();
        return BuildOutcome();
    }

    /// <summary>Asks the run to stop. The current phase ends at once and everything is restored.</summary>
    public void Stop()
    {
        Abort(AbortReason.UserStop);
    }

    /// <summary>Runs the phases and turns any error into a failure of the run.</summary>
    private async Task RunGuardedAsync()
    {
        try { await PrepareAndRunAsync(); }
        catch (Exception ex) { RecordFailure(ex); }
    }

    /// <summary>Prepares the run and runs the phases.</summary>
    private async Task PrepareAndRunAsync()
    {
        await PrepareAsync();
        await RunPhasesAsync();
    }

    /// <summary>Keeps the laptop awake, starts to listen for samples and remembers the brightness to restore.</summary>
    private async Task PrepareAsync()
    {
        startedAt = DateTime.Now;
        KeepAwake.On();
        sampling.SampleReady += OnSampleReady;
        originalBrightness = await Task.Run(brightness.TryReadPercent);
    }

    /// <summary>Runs phase after phase until all are done or the run is stopped.</summary>
    private async Task RunPhasesAsync()
    {
        while (HasNextPhase()) { await RunNextPhaseAsync(); }
    }

    /// <summary>Tells whether another phase should start.</summary>
    /// <returns>True when the run was not stopped and phases are left.</returns>
    private bool HasNextPhase() => abortReason == AbortReason.None && nextPhaseIndex < PhaseCatalogue.All.Count;

    /// <summary>Runs one phase: set it up, let its clock run, tear it down.</summary>
    private async Task RunNextPhaseAsync()
    {
        int index = nextPhaseIndex++;
        currentIndex = index;
        PhaseDefinition phase = PhaseCatalogue.All[index];
        await ApplyPhaseAsync(phase);
        StartPhaseClock(index, phase);
        await WaitForPhaseEndAsync(phase);
        FinishPhase(index);
    }

    /// <summary>Sets the brightness and starts the load of a phase. For the display-off phase it also waits until the display is off.</summary>
    /// <param name="phase">The phase.</param>
    private async Task ApplyPhaseAsync(PhaseDefinition phase)
    {
        await ApplyBrightnessAsync(phase);
        StartLoad(phase.Load);
        if (phase.Load != PhaseLoad.DisplayOff) { return; }
        await TurnDisplayOffAsync(phase);
    }

    /// <summary>Sets the brightness of a phase on a pool thread (a WMI call can take a moment). A failure becomes a note.</summary>
    /// <param name="phase">The phase.</param>
    private async Task ApplyBrightnessAsync(PhaseDefinition phase)
    {
        if (phase.BrightnessPercent is not int percent) { return; }
        string? error = await Task.Run(() => brightness.TrySetPercent(percent));
        if (error != null) { AddNote(phase, $"brightness could not be set ({error})"); }
    }

    /// <summary>Starts the load of a phase.</summary>
    /// <param name="load">The load.</param>
    private void StartLoad(PhaseLoad load)
    {
        if (load == PhaseLoad.CpuHalf) { cpuLoad.Start(Math.Max(1, Environment.ProcessorCount / 2)); }
        if (load == PhaseLoad.CpuFull) { cpuLoad.Start(Environment.ProcessorCount); }
        if (load == PhaseLoad.Graphics) { ShowGraphics(); }
    }

    /// <summary>Opens the full-screen animation window.</summary>
    private void ShowGraphics()
    {
        graphics = new GraphicsWindow();
        graphics.StopRequested += OnGraphicsStopRequested;
        graphics.Show();
    }

    /// <summary>Handles Esc in the graphics window.</summary>
    /// <param name="sender">The window.</param>
    /// <param name="e">Not used.</param>
    private void OnGraphicsStopRequested(object? sender, EventArgs e)
    {
        Stop();
    }

    /// <summary>
    /// Turns the display off and waits until Windows reports that it is off. The command is sent a few seconds after the graphics
    /// window has closed, so the window change cannot wake the display. If the display is not off 3 seconds after the command,
    /// the broadcast fall-back is sent once. If it is still not off after 10 seconds, the run ends with "Invalid: display stayed on".
    /// From now on the program must not touch the mouse or keyboard.
    /// </summary>
    /// <param name="phase">The display-off phase, for a note when the display state cannot be read.</param>
    private async Task TurnDisplayOffAsync(PhaseDefinition phase)
    {
        KeepAwake.SystemOnly();
        displayIsOff = true;
        if (!await PauseAsync(TestSettings.DisplayOffDelaySeconds)) { return; }
        DisplayPower.TurnOff(windowHandle);
        bool off = await DisplayStateMonitor.WaitForOffAsync(TimeSpan.FromSeconds(TestSettings.DisplayFirstWaitSeconds), runCancel.Token);
        if (!off) { off = await RetryDisplayOffAsync(); }
        if (off || abortReason != AbortReason.None) { return; }
        NoteUnreadableDisplay(phase);
        Abort(AbortReason.DisplayStayedOn);
    }

    /// <summary>Sends the broadcast fall-back once and waits for the rest of the time limit.</summary>
    /// <returns>True when the display reported "off".</returns>
    private async Task<bool> RetryDisplayOffAsync()
    {
        DisplayPower.TurnOffBroadcast();
        double restSeconds = TestSettings.DisplayGiveUpSeconds - TestSettings.DisplayFirstWaitSeconds;
        return await DisplayStateMonitor.WaitForOffAsync(TimeSpan.FromSeconds(restSeconds), runCancel.Token);
    }

    /// <summary>Waits for a number of seconds, or until the run is stopped.</summary>
    /// <param name="seconds">How long to wait.</param>
    /// <returns>True after the full time; false when the run was stopped.</returns>
    private async Task<bool> PauseAsync(double seconds)
    {
        try { await Task.Delay(TimeSpan.FromSeconds(seconds), runCancel.Token); return true; }
        catch (OperationCanceledException) { return false; }
    }

    /// <summary>Adds a note when no display state ever arrived, because then "stayed on" may be a failed registration.</summary>
    /// <param name="phase">The phase.</param>
    private void NoteUnreadableDisplay(PhaseDefinition phase)
    {
        if (DisplayStateMonitor.Current != DisplayStateKind.Unknown) { return; }
        AddNote(phase, "the display state could not be read (no power notification arrived)");
    }

    /// <summary>Starts the clock of a phase: samples from now on belong to it, and the window starts its progress bar.</summary>
    /// <param name="index">Index of the phase.</param>
    /// <param name="phase">The phase.</param>
    private void StartPhaseClock(int index, PhaseDefinition phase)
    {
        statuses[index] = PhaseStatus.Running;
        phaseStartSeconds = ProcessClock.Seconds;
        activePhase = phase;
        PhaseStarted?.Invoke(this, new PhaseEventArgs(index, PhaseStatus.Running));
    }

    /// <summary>Waits for the length of the phase, or until the run is stopped.</summary>
    /// <param name="phase">The phase.</param>
    private async Task WaitForPhaseEndAsync(PhaseDefinition phase)
    {
        try { await Task.Delay(TimeSpan.FromSeconds(phase.Seconds), runCancel.Token); }
        catch (OperationCanceledException) { /* A stop request cancels the wait; FinishPhase reads the reason. */ }
    }

    /// <summary>Ends a phase: stops its load, sets its final status and tells the window. A second call for the same phase does nothing.</summary>
    /// <param name="index">Index of the phase.</param>
    private void FinishPhase(int index)
    {
        if (closed[index]) { return; }
        closed[index] = true;
        activePhase = null;
        StopLoad();
        PhaseStatus final = abortReason == AbortReason.None ? PhaseStatus.Complete : StatusFor(abortReason);
        statuses[index] = final;
        PhaseEnded?.Invoke(this, new PhaseEventArgs(index, final));
    }

    /// <summary>Stops the CPU load, closes the graphics window and turns the display on. Safe to call at any time.</summary>
    private void StopLoad()
    {
        cpuLoad.Stop();
        CloseGraphics();
        RestoreDisplay();
    }

    /// <summary>Closes the graphics window when it is open.</summary>
    private void CloseGraphics()
    {
        if (graphics == null) { return; }
        GraphicsWindow window = graphics;
        graphics = null;
        window.StopRequested -= OnGraphicsStopRequested;
        window.CloseScene();
    }

    /// <summary>Turns the display on again when this run turned it off.</summary>
    private void RestoreDisplay()
    {
        if (!displayIsOff) { return; }
        displayIsOff = false;
        DisplayPower.TurnOn();
        KeepAwake.On();
    }

    /// <summary>Puts everything back: stops listening, puts the power settings back, stops the load, restores the brightness and ends the keep-awake request.</summary>
    private async Task RestoreAsync()
    {
        sampling.SampleReady -= OnSampleReady;
        activePhase = null;
        // The power settings come first: they are the change that outlives the program if anything below throws.
        await RestorePowerAsync();
        StopLoad();
        await RestoreBrightnessAsync();
        KeepAwake.Off();
    }

    /// <summary>Puts the processor power settings back on a pool thread. A failure is reported in the outcome.</summary>
    private async Task RestorePowerAsync()
    {
        PowerRunRecord record = await Task.Run(power.Restore);
        if (record.RestoreProblem.Length == 0) { return; }
        restoreProblems.Add($"the processor power settings could not be put back ({record.RestoreProblem}); the backup file {PowerSettingsBackup.FilePath()} was kept");
    }

    /// <summary>Sets the brightness that the laptop had before the test. A failure is reported in the outcome.</summary>
    private async Task RestoreBrightnessAsync()
    {
        if (originalBrightness is not int original) { return; }
        string? error = await Task.Run(() => brightness.TrySetPercent(original));
        if (error != null) { restoreProblems.Add($"the original brightness of {original}% could not be restored ({error})"); }
    }

    /// <summary>Takes a new sample: stamps it, keeps it and checks the safety limits. An error here stops the run.</summary>
    /// <param name="sender">The sampling loop.</param>
    /// <param name="sample">The new sample.</param>
    private void OnSampleReady(object? sender, Sample sample)
    {
        try { Accept(sample); }
        catch (Exception ex) { RecordFailure(ex); }
    }

    /// <summary>Stamps, keeps and checks one sample.</summary>
    /// <param name="sample">The sample.</param>
    private void Accept(Sample sample)
    {
        StampSample(sample);
        samples.Add(sample);
        SampleAccepted?.Invoke(this, sample);
        CheckSafety(sample);
    }

    /// <summary>Marks the sample with the running phase and the time since the phase clock started.</summary>
    /// <param name="sample">The sample.</param>
    private void StampSample(Sample sample)
    {
        if (activePhase == null) { return; }
        sample.PhaseNumber = activePhase.Number;
        sample.SecondsIntoPhase = sample.MonotonicSeconds - phaseStartSeconds;
    }

    /// <summary>Stops the run when the charger is plugged in, the battery is low, or the user touched the laptop with the display off.</summary>
    /// <param name="sample">The sample.</param>
    private void CheckSafety(Sample sample)
    {
        if (sample.OnCharger() == true) { Abort(AbortReason.Charger); return; }
        if (sample.BatteryPercent < TestSettings.MinimumBatteryPercent) { Abort(AbortReason.BatteryLow); return; }
        if (InputDuringDisplayOff(sample)) { Abort(AbortReason.UserInput); return; }
        if (DisplayWokeUp(sample)) { Abort(AbortReason.DisplayWoke); }
    }

    /// <summary>
    /// Tells whether the display reported "on" or "dimmed" during the display-off phase. The input check runs first, because
    /// a touch on the laptop wakes the display and "input" explains it better than "woke up".
    /// </summary>
    /// <param name="sample">The sample.</param>
    /// <returns>True when the sample was read after the phase clock started and the display was not off or unknown.</returns>
    private bool DisplayWokeUp(Sample sample)
    {
        if (activePhase?.Load != PhaseLoad.DisplayOff) { return false; }
        if (sample.SecondsIntoPhase < 0) { return false; }
        return sample.DisplayState == DisplayStateKind.On || sample.DisplayState == DisplayStateKind.Dimmed;
    }

    /// <summary>Tells whether there was mouse or keyboard input after the display was switched off.</summary>
    /// <param name="sample">The sample.</param>
    /// <returns>True when the idle time is shorter than the time the display has been off (with a few seconds of tolerance).</returns>
    private bool InputDuringDisplayOff(Sample sample)
    {
        if (activePhase?.Load != PhaseLoad.DisplayOff) { return false; }
        double? idle = sample.Get(SampleKeys.UserIdle);
        return idle < sample.SecondsIntoPhase - TestSettings.InputGraceSeconds;
    }

    /// <summary>Records the reason and cancels the wait of the current phase. Only the first reason counts.</summary>
    /// <param name="reason">Why the run must stop.</param>
    private void Abort(AbortReason reason)
    {
        if (abortReason != AbortReason.None) { return; }
        abortReason = reason;
        runCancel.Cancel();
    }

    /// <summary>Records an error as the reason of the stop and closes the phase that was open.</summary>
    /// <param name="ex">The error. Only its type name is kept.</param>
    private void RecordFailure(Exception ex)
    {
        failureText = Reasons.From(ex);
        Abort(AbortReason.Failure);
        CloseOpenPhase();
    }

    /// <summary>Closes the phase that was being set up or run, if any.</summary>
    private void CloseOpenPhase()
    {
        if (currentIndex < 0) { return; }
        FinishPhase(currentIndex);
    }

    /// <summary>Adds a note to a phase.</summary>
    /// <param name="phase">The phase.</param>
    /// <param name="text">The note.</param>
    private void AddNote(PhaseDefinition phase, string text)
    {
        int index = phase.Number - 1;
        notes[index] = notes[index].Length == 0 ? text : $"{notes[index]}; {text}";
    }

    /// <summary>Gives the phase status that belongs to a stop reason.</summary>
    /// <param name="reason">The reason.</param>
    /// <returns>The status of the phase that was open.</returns>
    private static PhaseStatus StatusFor(AbortReason reason)
    {
        if (reason == AbortReason.Charger) { return PhaseStatus.InvalidCharger; }
        if (reason == AbortReason.UserInput) { return PhaseStatus.InvalidInput; }
        if (reason == AbortReason.BatteryLow) { return PhaseStatus.StoppedBatteryLow; }
        if (reason == AbortReason.Failure) { return PhaseStatus.Failed; }
        if (reason == AbortReason.DisplayStayedOn) { return PhaseStatus.InvalidDisplayOn; }
        if (reason == AbortReason.DisplayWoke) { return PhaseStatus.InvalidDisplayWoke; }
        return PhaseStatus.StoppedByUser;
    }

    /// <summary>Describes how the run ended, as a sentence.</summary>
    /// <returns>The sentence.</returns>
    private string ReasonText()
    {
        if (abortReason == AbortReason.None) { return "Finished all phases."; }
        if (abortReason == AbortReason.UserStop) { return "Stopped by the user."; }
        if (abortReason == AbortReason.Charger) { return "Stopped: the charger was plugged in during a phase."; }
        if (abortReason == AbortReason.BatteryLow) { return $"Stopped: the battery fell below {TestSettings.MinimumBatteryPercent}%."; }
        if (abortReason == AbortReason.UserInput) { return "Stopped: the mouse or keyboard was used while the display was off."; }
        if (abortReason == AbortReason.DisplayStayedOn) { return "Stopped: the display did not turn off, so the Screen off phase is not valid."; }
        if (abortReason == AbortReason.DisplayWoke) { return "Stopped: the display woke up during the Screen off phase."; }
        return $"Stopped by an error ({failureText}).";
    }

    /// <summary>Collects everything the run produced.</summary>
    /// <returns>The outcome.</returns>
    private TestOutcome BuildOutcome()
    {
        return new TestOutcome(startedAt, samples.ToList(), statuses.ToList(), notes.ToList(), abortReason, ReasonText(), restoreProblems.ToList());
    }
}
