// GraphicsWindow.xaml.cs: code behind of the full-screen graphics window, and the SceneBuilder that fills it with shapes.
// Who uses it: TestRunner (Show at the start of the graphics phase, CloseScene at the end).
// Graphics on the GPU: each shape gets a RenderTransform (rotate, scale, translate) animated by a Storyboard of DoubleAnimations,
// and every few shapes a BlurEffect, which is a pixel shader. No timer redraws anything and no layout changes while it runs.
// The scene is the same on every run: the shapes are placed by random numbers with a fixed seed (TestSettings.GraphicsSeed).
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Effects;
using System.Windows.Shapes;

namespace EnergyCalibrator;

/// <summary>The full-screen window of the graphics phase.</summary>
public partial class GraphicsWindow : Window
{
    private Storyboard? scene;

    /// <summary>Creates the window. The shapes are added when it has loaded and knows its size.</summary>
    public GraphicsWindow()
    {
        InitializeComponent();
        Notice.Text = $"Graphics phase. Please do not touch the laptop. When it ends, the display turns off by itself for {TestSettings.PhaseSeconds / 60.0:0.#} minutes. Press Esc to stop the test.";
    }

    /// <summary>Raised when the user presses Esc.</summary>
    internal event EventHandler? StopRequested;

    /// <summary>Stops the animations and closes the window. Safe to call twice.</summary>
    internal void CloseScene()
    {
        try { scene?.Stop(this); Close(); }
        catch (InvalidOperationException) { /* The window is already closing. */ }
    }

    /// <summary>
    /// Builds the shapes and starts the storyboard. If WPF cannot start the animations, the overlay says so instead of a dialog
    /// opening in the middle of a test; the phase then shows a very low GPU use and gets a warning in the result.
    /// </summary>
    /// <param name="sender">The window.</param>
    /// <param name="e">Not used.</param>
    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        try { StartScene(); }
        catch (Exception ex) { Notice.Text = $"The animation could not start ({ex.GetType().Name}). Press Esc to stop the test."; }
    }

    /// <summary>Builds the scene for the size of the window and begins its storyboard.</summary>
    private void StartScene()
    {
        scene = SceneBuilder.Build(Scene, ActualWidth, ActualHeight);
        scene.Begin(this, true);
    }

    /// <summary>Turns Esc into a stop request.</summary>
    /// <param name="sender">The window.</param>
    /// <param name="e">The key.</param>
    private void OnKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Escape) { return; }
        StopRequested?.Invoke(this, EventArgs.Empty);
    }
}

/// <summary>Creates the shapes and the storyboard of the graphics scene.</summary>
internal static class SceneBuilder
{
    private const string AnglePath = "(0).(1)[0].(2)";
    private const string ScalePath = "(0).(1)[1].(2)";
    private const string MovePath = "(0).(1)[2].(2)";

    /// <summary>Adds the shapes to a canvas and builds the storyboard that animates them.</summary>
    /// <param name="canvas">The canvas that holds the shapes.</param>
    /// <param name="width">Width of the screen in device-independent pixels.</param>
    /// <param name="height">Height of the screen in device-independent pixels.</param>
    /// <returns>The storyboard; the caller begins it.</returns>
    public static Storyboard Build(Canvas canvas, double width, double height)
    {
        Random random = new(TestSettings.GraphicsSeed);
        Storyboard board = new();
        for (int i = 0; i < TestSettings.GraphicsShapeCount; i++) { AddShape(canvas, board, random, i, width, height); }
        return board;
    }

    /// <summary>Creates one shape, places it, gives it transforms and animations.</summary>
    /// <param name="canvas">The canvas.</param>
    /// <param name="board">The storyboard.</param>
    /// <param name="random">The random numbers.</param>
    /// <param name="index">Number of the shape.</param>
    /// <param name="width">Screen width.</param>
    /// <param name="height">Screen height.</param>
    private static void AddShape(Canvas canvas, Storyboard board, Random random, int index, double width, double height)
    {
        double size = 40 + (random.NextDouble() * 160);
        Shape shape = NewShape(index, size);
        shape.Fill = NewBrush(random);
        shape.Opacity = 0.5 + (random.NextDouble() * 0.4);
        shape.RenderTransformOrigin = new Point(0.5, 0.5);
        shape.RenderTransform = new TransformGroup { Children = { new RotateTransform(), new ScaleTransform(1, 1), new TranslateTransform() } };
        shape.Effect = EffectFor(index);
        Canvas.SetLeft(shape, random.NextDouble() * width);
        Canvas.SetTop(shape, random.NextDouble() * height);
        canvas.Children.Add(shape);
        AddAnimations(board, shape, random);
    }

    /// <summary>Makes a shape of one of three kinds: ellipse, rounded rectangle or triangle.</summary>
    /// <param name="index">Number of the shape.</param>
    /// <param name="size">Width and height in device-independent pixels.</param>
    /// <returns>The shape.</returns>
    private static Shape NewShape(int index, double size)
    {
        if (index % 3 == 0) { return new Ellipse { Width = size, Height = size }; }
        if (index % 3 == 1) { return new Rectangle { Width = size, Height = size, RadiusX = 10, RadiusY = 10 }; }
        return new Polygon { Width = size, Height = size, Stretch = Stretch.Fill, Points = [new Point(0.5, 0), new Point(1, 1), new Point(0, 1)] };
    }

    /// <summary>Makes a frozen radial gradient from two random colours.</summary>
    /// <param name="random">The random numbers.</param>
    /// <returns>The brush.</returns>
    private static Brush NewBrush(Random random)
    {
        Color inner = Color.FromRgb((byte)random.Next(80, 256), (byte)random.Next(80, 256), (byte)random.Next(80, 256));
        Color outer = Color.FromRgb((byte)random.Next(0, 160), (byte)random.Next(0, 160), (byte)random.Next(0, 160));
        RadialGradientBrush brush = new(inner, outer);
        brush.Freeze();
        return brush;
    }

    /// <summary>Gives every n-th shape a blur effect (a pixel shader that loads the graphics chip).</summary>
    /// <param name="index">Number of the shape.</param>
    /// <returns>The effect, or null for the other shapes.</returns>
    private static Effect? EffectFor(int index)
    {
        if (index % TestSettings.GraphicsBlurEveryNth != 0) { return null; }
        return new BlurEffect { Radius = TestSettings.GraphicsBlurRadius };
    }

    /// <summary>Adds the five animations of a shape: rotation, scale in x and y, move in x and y.</summary>
    /// <param name="board">The storyboard.</param>
    /// <param name="shape">The shape.</param>
    /// <param name="random">The random numbers.</param>
    private static void AddAnimations(Storyboard board, Shape shape, Random random)
    {
        double seconds = 3 + (random.NextDouble() * 6);
        double zoom = 0.5 + (random.NextDouble() * 1.2);
        double travelX = (random.NextDouble() - 0.5) * 500;
        double travelY = (random.NextDouble() - 0.5) * 300;
        AddAnimation(board, shape, AnglePath, RotateTransform.AngleProperty, new DoubleAnimation(0, 360, TimeSpan.FromSeconds(seconds * 2)));
        AddAnimation(board, shape, ScalePath, ScaleTransform.ScaleXProperty, Swing(0.4, zoom, seconds));
        AddAnimation(board, shape, ScalePath, ScaleTransform.ScaleYProperty, Swing(0.4, zoom, seconds));
        AddAnimation(board, shape, MovePath, TranslateTransform.XProperty, Swing(-travelX, travelX, seconds));
        AddAnimation(board, shape, MovePath, TranslateTransform.YProperty, Swing(-travelY, travelY, seconds));
    }

    /// <summary>Makes an animation that goes from one value to another and back, forever.</summary>
    /// <param name="from">Start value.</param>
    /// <param name="to">End value.</param>
    /// <param name="seconds">Seconds for one way.</param>
    /// <returns>The animation.</returns>
    private static DoubleAnimation Swing(double from, double to, double seconds)
    {
        return new DoubleAnimation(from, to, TimeSpan.FromSeconds(seconds)) { AutoReverse = true };
    }

    /// <summary>Points an animation at one property of one transform of a shape and adds it to the storyboard.</summary>
    /// <param name="board">The storyboard.</param>
    /// <param name="shape">The shape.</param>
    /// <param name="path">The property path with an index into the transform group, for example "(0).(1)[0].(2)".</param>
    /// <param name="leaf">The animated property of the transform.</param>
    /// <param name="animation">The animation.</param>
    private static void AddAnimation(Storyboard board, Shape shape, string path, DependencyProperty leaf, DoubleAnimation animation)
    {
        animation.RepeatBehavior = RepeatBehavior.Forever;
        Storyboard.SetTarget(animation, shape);
        Storyboard.SetTargetProperty(animation, new PropertyPath(path, UIElement.RenderTransformProperty, TransformGroup.ChildrenProperty, leaf));
        board.Children.Add(animation);
    }
}
