// MainWindow.xaml.cs: code behind of the main window.
// Who uses it: App.xaml opens it. It owns the SamplingLoop (live readings every 2 seconds, from the moment the window opens),
// starts a TestRunner when the user presses Start, shows the phase list and the results, and saves the result files.
// How the Start button is decided: every sample re-checks it (StartGate), so there is no extra loop or timer for it.
// Power settings: at start the window restores a backup left by a crashed run; Start first shows a warning (StartWarning) and a
// PowerSettingsSession changes the processor settings for the run; the settings are put back when the run ends and when the window closes.
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Interop;

namespace EnergyCalibrator;

/// <summary>The main window.</summary>
public partial class MainWindow : Window
{
    private readonly SamplingLoop sampling = new();
    private readonly List<ReadingRow> readingRows = ReadingRows.Create();
    private readonly List<PhaseRowView> phaseRows = new();
    private readonly ObservableCollection<PhaseResultRow> resultRows = new();
    private readonly ObservableCollection<string> modelLines = new();
    private MachineInfo machine = MachineInfo.Unknown;
    private TestRunner? runner;
    private RunRecorder? recorder;
    private Sample? latestSample;
    private string? resultsFolder;
    private bool testRunning;
    private bool closeAfterTest;
    private bool confirming;
    private bool runFinished;
    private bool lastCanStart;
    private PowerSettingsSession? powerSession;
    private string startupNote = "";

    /// <summary>Creates the window and its XAML parts.</summary>
    public MainWindow()
    {
        InitializeComponent();
    }

    /// <summary>Starts to listen for the display state as soon as the window has a handle, before the first sample is taken.</summary>
    /// <param name="e">Passed on.</param>
    protected override void OnSourceInitialized(EventArgs e)
    {
        base.OnSourceInitialized(e);
        DisplayStateMonitor.Attach(this);
    }

    /// <summary>Puts the power settings back if a run left them changed, and ends the display state registration when the window is gone.</summary>
    /// <param name="e">Passed on.</param>
    protected override void OnClosed(EventArgs e)
    {
        powerSession?.Restore();
        DisplayStateMonitor.Detach();
        base.OnClosed(e);
    }

    /// <summary>Fills the window, restores power settings left by a crashed run, and starts the live readings.</summary>
    /// <param name="sender">The window.</param>
    /// <param name="e">Not used.</param>
    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        ReadingsList.ItemsSource = readingRows;
        ResultsGrid.ItemsSource = resultRows;
        ModelList.ItemsSource = modelLines;
        InstructionText.Text = InstructionTexts.Build();
        BuildPhaseRows();
        AdminButton.Visibility = Elevation.IsElevated() ? Visibility.Collapsed : Visibility.Visible;
        startupNote = await Task.Run(new PowerSettingsSession().Recover);
        sampling.SampleReady += OnSampleReady;
        sampling.Start();
        _ = LoadMachineInfoAsync();
    }

    /// <summary>Reads maker, model and processor on a pool thread.</summary>
    private async Task LoadMachineInfoAsync()
    {
        machine = await Task.Run(MachineInfo.Read);
    }

    /// <summary>Adds one row to the phase list for every phase.</summary>
    private void BuildPhaseRows()
    {
        foreach (PhaseDefinition phase in PhaseCatalogue.All) { AddPhaseRow(phase); }
    }

    /// <summary>Adds the row of one phase.</summary>
    /// <param name="phase">The phase.</param>
    private void AddPhaseRow(PhaseDefinition phase)
    {
        PhaseRowView row = new(phase);
        phaseRows.Add(row);
        PhaseStack.Children.Add(row.Root);
    }

    /// <summary>Shows a new sample. An error while showing it is reported in the status line and does not stop the loop.</summary>
    /// <param name="sender">The sampling loop.</param>
    /// <param name="sample">The new sample.</param>
    private void OnSampleReady(object? sender, Sample sample)
    {
        try { ShowSample(sample); }
        catch (Exception ex) { StatusText.Text = $"Internal error while showing a reading ({ex.GetType().Name})."; }
    }

    /// <summary>Updates the live readings and the Start button.</summary>
    /// <param name="sample">The new sample.</param>
    private void ShowSample(Sample sample)
    {
        latestSample = sample;
        foreach (ReadingRow row in readingRows) { row.Update(sample); }
        UpdateStartState();
    }

    /// <summary>Enables or disables Start and shows why. It does nothing while a test runs or while the start warning is open.</summary>
    private void UpdateStartState()
    {
        if (testRunning || confirming) { return; }
        (bool canStart, string message) = StartGate.Evaluate(latestSample);
        lastCanStart = canStart;
        StartButton.IsEnabled = canStart;
        StatusText.Text = startupNote.Length == 0 ? message : $"{startupNote} {message}";
    }

    /// <summary>Warns the user, and starts the test when the user presses OK.</summary>
    /// <param name="sender">The button.</param>
    /// <param name="e">Not used.</param>
    private async void OnStartClick(object sender, RoutedEventArgs e)
    {
        confirming = true;
        StartButton.IsEnabled = false;
        bool confirmed = await ConfirmStartAsync();
        confirming = false;
        if (!confirmed) { UpdateStartState(); return; }
        await RunTestSafelyAsync();
    }

    /// <summary>Shows the warning about the battery use and the power settings.</summary>
    /// <returns>True when the user pressed OK. Cancel changes nothing.</returns>
    private async Task<bool> ConfirmStartAsync()
    {
        double? fullChargeMwh = await Task.Run(BatteryFullCharge.TryReadMwh);
        string text = StartWarning.Build(fullChargeMwh);
        MessageBoxResult answer = MessageBox.Show(this, text, "Energy Calibrator", MessageBoxButton.OKCancel, MessageBoxImage.Warning, MessageBoxResult.Cancel);
        return answer == MessageBoxResult.OK;
    }

    /// <summary>Runs the test, reports any error, puts the power settings back, and always brings the window back to the idle state.</summary>
    private async Task RunTestSafelyAsync()
    {
        try { await RunTestAsync(); }
        catch (Exception ex) { ReportFailure(ex); }
        await RestorePowerSafelyAsync();
        EndRunUi();
    }

    /// <summary>
    /// Puts the power settings back once more after the run. The runner has done it already, so this does nothing then; it covers an
    /// error that was thrown before the runner started or while it cleaned up. A failure is shown in a message box.
    /// </summary>
    private async Task RestorePowerSafelyAsync()
    {
        if (powerSession == null) { return; }
        PowerRunRecord record = await Task.Run(powerSession.Restore);
        if (record.RestoreProblem.Length == 0) { return; }
        string text = $"The processor power settings could not be put back ({record.RestoreProblem}). The backup file stays at {PowerSettingsBackup.FilePath()}; the program tries again when it starts next time.";
        MessageBox.Show(this, text, "Energy Calibrator", MessageBoxButton.OK, MessageBoxImage.Warning);
    }

    /// <summary>Creates the results folder, changes the power settings, runs the test, and shows and saves the results.</summary>
    private async Task RunTestAsync()
    {
        resultsFolder = ResultFolder.CreateNew();
        recorder = new RunRecorder(resultsFolder);
        BeginRunUi();
        ReportSaveProblem(recorder.Start());
        powerSession = new PowerSettingsSession();
        await BeginPowerAsync(powerSession);
        runner = new TestRunner(sampling, new WindowInteropHelper(this).Handle, powerSession);
        runner.PhaseStarted += OnPhaseStarted;
        runner.PhaseEnded += OnPhaseEnded;
        runner.SampleAccepted += OnSampleAccepted;
        TestOutcome outcome = await runner.RunAsync();
        FinishRun(outcome);
    }

    /// <summary>
    /// Sets the processor power settings to maximum performance on battery. If that fails, a message box gives the reason and the test
    /// goes on with the settings unchanged (the record says so, and summary.txt shows it).
    /// </summary>
    /// <param name="session">The session of this run.</param>
    private async Task BeginPowerAsync(PowerSettingsSession session)
    {
        StatusText.Text = "Setting the processor power settings for the test...";
        PowerRunRecord record = await Task.Run(session.Begin);
        if (record.ChangeProblem.Length == 0) { return; }
        string text = $"The processor power settings could not be changed ({record.ChangeProblem}). The test runs with the settings unchanged, so the CPU 100% phase may not show full power.";
        MessageBox.Show(this, text, "Energy Calibrator", MessageBoxButton.OK, MessageBoxImage.Warning);
    }

    /// <summary>Puts the window in the "running" state.</summary>
    private void BeginRunUi()
    {
        testRunning = true;
        StartButton.IsEnabled = false;
        StopButton.IsEnabled = true;
        AdminButton.IsEnabled = false;
        FolderButton.IsEnabled = false;
        resultRows.Clear();
        modelLines.Clear();
        ResetPhaseRows();
        Tabs.SelectedIndex = 0;
    }

    /// <summary>Puts every phase row back to "waiting".</summary>
    private void ResetPhaseRows()
    {
        foreach (PhaseRowView row in phaseRows) { row.Reset(); }
    }

    /// <summary>Puts the window back in the idle state and closes it when the user tried to close it during the test.</summary>
    private void EndRunUi()
    {
        testRunning = false;
        runner = null;
        StopButton.IsEnabled = false;
        AdminButton.IsEnabled = true;
        FolderButton.IsEnabled = true;
        UpdateStartState();
        if (closeAfterTest) { Close(); }
    }

    /// <summary>Shows that a phase started.</summary>
    /// <param name="sender">The runner.</param>
    /// <param name="e">The phase.</param>
    private void OnPhaseStarted(object? sender, PhaseEventArgs e)
    {
        phaseRows[e.Index].Start();
        StatusText.Text = $"Phase {e.Index + 1} of {PhaseCatalogue.All.Count}: {PhaseCatalogue.All[e.Index].Name}";
    }

    /// <summary>Shows that a phase ended.</summary>
    /// <param name="sender">The runner.</param>
    /// <param name="e">The phase and its final status.</param>
    private void OnPhaseEnded(object? sender, PhaseEventArgs e)
    {
        phaseRows[e.Index].Finish(e.Status);
        SavePhaseSafely(e.Index);
    }

    /// <summary>Appends the row of a finished phase to phases.csv. An error is reported in the status line and never stops the test.</summary>
    /// <param name="index">Index of the phase.</param>
    private void SavePhaseSafely(int index)
    {
        try { AppendPhaseRow(index); }
        catch (Exception ex) { StatusText.Text = $"Data could not be saved to phases.csv ({ex.GetType().Name}); the test goes on."; }
    }

    /// <summary>Works out the result of a phase from what the run has measured so far and appends it to phases.csv.</summary>
    /// <param name="index">Index of the phase.</param>
    private void AppendPhaseRow(int index)
    {
        if (runner == null) { return; }
        PhaseResult result = PhaseAnalyser.Analyse(index, runner.Snapshot());
        ReportSaveProblem(recorder?.AppendPhase(result));
    }

    /// <summary>Appends a new sample to samples.csv.</summary>
    /// <param name="sender">The runner.</param>
    /// <param name="sample">The sample, stamped with its phase.</param>
    private void OnSampleAccepted(object? sender, Sample sample)
    {
        ReportSaveProblem(recorder?.AppendSample(sample));
    }

    /// <summary>Shows why data could not be saved. Does nothing when there was no problem.</summary>
    /// <param name="problem">The reason, or null.</param>
    private void ReportSaveProblem(string? problem)
    {
        if (problem == null) { return; }
        StatusText.Text = problem;
    }

    /// <summary>Works out and shows the results, saves the files, and shows the final message.</summary>
    /// <param name="outcome">What the run produced.</param>
    private void FinishRun(TestOutcome outcome)
    {
        IReadOnlyList<PhaseResult> results = PhaseAnalyser.AnalyseAll(outcome);
        PowerModel model = PowerModel.Build(results);
        ShowResults(results, model);
        string saved = SaveResults(outcome, results, model);
        string published = PublishModel(outcome, results, model);
        StatusText.Text = $"{outcome.ReasonText} {saved} {published}";
        Tabs.SelectedIndex = 1;
    }

    /// <summary>Writes the model file for the service (or decides not to) and sets the exit code of the program.</summary>
    /// <param name="outcome">What the run produced.</param>
    /// <param name="results">One result per phase.</param>
    /// <param name="model">The power model.</param>
    /// <returns>A sentence that says what happened to the model file.</returns>
    private string PublishModel(TestOutcome outcome, IReadOnlyList<PhaseResult> results, PowerModel model)
    {
        PowerRunRecord power = powerSession?.Record ?? new PowerRunRecord();
        ModelPublication publication = ServiceModel.Publish(new RunReport(machine, outcome, results, model, power), App.DataDir, DateTime.UtcNow);
        runFinished = true;
        ExitCodes.Current = publication.ExitCode;
        return publication.Message;
    }

    /// <summary>Fills the results table and the model lines.</summary>
    /// <param name="results">One result per phase.</param>
    /// <param name="model">The power model.</param>
    private void ShowResults(IReadOnlyList<PhaseResult> results, PowerModel model)
    {
        foreach (PhaseResult result in results) { resultRows.Add(PhaseResultRow.From(result)); }
        foreach (string line in model.DescribeLines()) { modelLines.Add(line); }
    }

    /// <summary>Writes the result files.</summary>
    /// <param name="outcome">What the run produced.</param>
    /// <param name="results">One result per phase.</param>
    /// <param name="model">The power model.</param>
    /// <returns>A sentence that says whether the files were saved.</returns>
    private string SaveResults(TestOutcome outcome, IReadOnlyList<PhaseResult> results, PowerModel model)
    {
        try
        {
            bool writeSamples = recorder?.SamplesNeedRewrite(outcome.Samples) ?? true;
            PowerRunRecord power = powerSession?.Record ?? new PowerRunRecord();
            ResultWriter.Save(resultsFolder!, new RunReport(machine, outcome, results, model, power), writeSamples);
            return "The results are saved; press \"Open results folder\".";
        }
        catch (Exception ex)
        {
            return $"The results could not be saved ({ex.GetType().Name}).";
        }
    }

    /// <summary>Shows an error that stopped the test or its start.</summary>
    /// <param name="ex">The error. Only its type and message are shown, on this screen only.</param>
    private void ReportFailure(Exception ex)
    {
        runFinished = true;
        ExitCodes.Current = ExitCodes.TestAborted;
        StatusText.Text = $"The test could not run: {ex.Message}";
        MessageBox.Show(StatusText.Text, "Energy Calibrator", MessageBoxButton.OK, MessageBoxImage.Warning);
    }

    /// <summary>Asks the running test to stop.</summary>
    /// <param name="sender">The button.</param>
    /// <param name="e">Not used.</param>
    private void OnStopClick(object sender, RoutedEventArgs e)
    {
        StatusText.Text = "Stopping and restoring the screen...";
        runner?.Stop();
    }

    /// <summary>Restarts the program with administrator rights, so Intel DTT can be read.</summary>
    /// <param name="sender">The button.</param>
    /// <param name="e">Not used.</param>
    private void OnAdminClick(object sender, RoutedEventArgs e)
    {
        if (!TryRestartElevated()) { return; }
        sampling.Stop();
        Application.Current.Shutdown();
    }

    /// <summary>Starts the elevated copy.</summary>
    /// <returns>True when the new copy was started; false when the user refused the Windows prompt.</returns>
    private bool TryRestartElevated()
    {
        try { Elevation.RestartElevated(); return true; }
        catch (Win32Exception)
        {
            StatusText.Text = "Administrator rights were not granted, so the program was not restarted.";
            return false;
        }
    }

    /// <summary>Opens the results folder of the last run, or the folder that holds all runs.</summary>
    /// <param name="sender">The button.</param>
    /// <param name="e">Not used.</param>
    private void OnFolderClick(object sender, RoutedEventArgs e)
    {
        string folder = resultsFolder ?? ResultFolder.Root();
        Directory.CreateDirectory(folder);
        Process.Start(new ProcessStartInfo("explorer.exe", $"\"{folder}\"") { UseShellExecute = true });
    }

    /// <summary>Stops a running test before the window closes, so the brightness and the display are restored.</summary>
    /// <param name="sender">The window.</param>
    /// <param name="e">Lets this method cancel the closing.</param>
    private void OnClosing(object? sender, CancelEventArgs e)
    {
        if (testRunning)
        {
            closeAfterTest = true;
            runner?.Stop();
            e.Cancel = true;
            return;
        }
        sampling.SampleReady -= OnSampleReady;
        sampling.Stop();
        if (runFinished) { return; }
        ExitCodes.Current = lastCanStart ? ExitCodes.StoppedByUser : ExitCodes.StartNotMet;
    }
}
