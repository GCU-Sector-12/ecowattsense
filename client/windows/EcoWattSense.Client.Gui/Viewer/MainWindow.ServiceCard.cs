// MainWindow.ServiceCard.cs: the dot and the "Last measurement" line of the Service card (viewer mode).
// Who uses it: RefreshViewer (the dot colour) and ShowHistory (the line and the pulse) in MainWindow.Viewer.cs.
// The dot is green while the service runs and grey otherwise. When a history refresh brings a new last row, the dot
// pulses once: a Storyboard of Opacity and ScaleTransform animations, which WPF runs on the render thread.
// No pulse when Windows has client area animations switched off, or when the window is hidden.
#nullable enable
using System.Collections.Generic;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Animation;
using EcoWattSense.Client.Gui.History;
using EcoWattSense.Client.Gui.Viewer;
using Brush = System.Windows.Media.Brush;
using Color = System.Windows.Media.Color;

namespace EcoWattSense.Client.Gui;

public partial class MainWindow
{
    private static readonly Brush RunningDotBrush = FrozenBrush(0x3F, 0xA3, 0x4D);
    private static readonly Brush StoppedDotBrush = FrozenBrush(0xB8, 0xC0, 0xCC);
    private static readonly Duration PulseHalf = new(TimeSpan.FromMilliseconds(500));

    private readonly NewRowTracker newRowTracker = new();
    private Storyboard? pulseStoryboard;

    /// <summary>Makes a frozen brush from three colour bytes.</summary>
    /// <param name="red">Red, 0 to 255.</param>
    /// <param name="green">Green, 0 to 255.</param>
    /// <param name="blue">Blue, 0 to 255.</param>
    /// <returns>The frozen brush.</returns>
    private static Brush FrozenBrush(byte red, byte green, byte blue)
    {
        var brush = new SolidColorBrush(Color.FromRgb(red, green, blue));
        brush.Freeze();
        return brush;
    }

    /// <summary>Colours the dot: green when the service runs, grey in every other state.</summary>
    /// <param name="state">The service state.</param>
    private void ShowServiceDot(ServiceState state)
        => ServiceDot.Fill = state == ServiceState.Running ? RunningDotBrush : StoppedDotBrush;

    /// <summary>Shows the time of the last measurement and pulses the dot when the last row is new.</summary>
    /// <param name="rows">The history rows of the last 24 hours.</param>
    private void ShowLastMeasurement(IReadOnlyList<HistoryRow> rows)
    {
        DateTime? lastEnd = MeasurementInfo.LastEndUtc(rows);
        LastMeasurementValue.Text = MeasurementInfo.Line(lastEnd, TimeZoneInfo.Local);
        bool isNew = newRowTracker.Observe(lastEnd);
        bool mayAnimate = SystemParameters.ClientAreaAnimation && IsVisible;
        if (isNew && mayAnimate) PulseDot();
    }

    /// <summary>Plays the pulse once: opacity down and scale up, and back, in about one second.</summary>
    private void PulseDot()
    {
        pulseStoryboard ??= BuildPulse();
        pulseStoryboard.Begin();
    }

    /// <summary>Builds the pulse storyboard once. It is reused for every pulse.</summary>
    /// <returns>The storyboard, which targets the dot.</returns>
    private Storyboard BuildPulse()
    {
        var storyboard = new Storyboard();
        storyboard.Children.Add(PulseAnimation(1.0, 0.35, "Opacity"));
        storyboard.Children.Add(PulseAnimation(1.0, 1.7, "(UIElement.RenderTransform).(ScaleTransform.ScaleX)"));
        storyboard.Children.Add(PulseAnimation(1.0, 1.7, "(UIElement.RenderTransform).(ScaleTransform.ScaleY)"));
        return storyboard;
    }

    /// <summary>Makes one animation of the pulse: from a value to another and back.</summary>
    /// <param name="from">The start value.</param>
    /// <param name="to">The value half way.</param>
    /// <param name="property">The property path on the dot.</param>
    /// <returns>The animation, with its target set to the dot.</returns>
    private DoubleAnimation PulseAnimation(double from, double to, string property)
    {
        var animation = new DoubleAnimation(from, to, PulseHalf) { AutoReverse = true };
        Storyboard.SetTarget(animation, ServiceDot);
        Storyboard.SetTargetProperty(animation, new PropertyPath(property));
        return animation;
    }
}
