// DonutChart.cs: the "Energy by state (24 h)" donut of the client window (SPEC 9).
// Who uses it: MainWindow puts one into the Border named DonutHost and calls SetRows after each history change.
// It draws a ring of WPF Path arcs split by state (Active, Idle, Unused), the total kWh in the centre and a legend with
// each state, its kWh and its percent. The arcs come from DonutGeometry. The shapes are built once per data change
// (no timer, no frame-by-frame drawing); WPF renders them on the GPU. Without data it shows an empty grey ring and "No data".
#nullable enable
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;
using Brush = System.Windows.Media.Brush;
using Color = System.Windows.Media.Color;
using ColorConverter = System.Windows.Media.ColorConverter;
using Point = System.Windows.Point;
using Rectangle = System.Windows.Shapes.Rectangle;
using Size = System.Windows.Size;
using Orientation = System.Windows.Controls.Orientation;

namespace EcoWattSense.Client.Gui.History
{
    /// <summary>A ring chart of the energy per state. Call SetRows to show new data.</summary>
    public sealed class DonutChart : UserControl
    {
        private const double RingSize = 170;
        private const double RingThickness = 22;
        private const double FullCircleDegrees = 359.99;

        private static readonly Brush TextBrush = Make("#728095");
        private static readonly Brush EmptyRingBrush = Make("#E3E8EE");
        private static readonly Brush ActiveBrush = Make(StateColours.Active);
        private static readonly Brush IdleBrush = Make(StateColours.Idle);
        private static readonly Brush UnusedBrush = Make(StateColours.Unused);

        private readonly Canvas ring = new() { Width = RingSize, Height = RingSize };
        private readonly TextBlock total = new() { FontSize = 22, FontWeight = FontWeights.SemiBold, HorizontalAlignment = HorizontalAlignment.Center };
        private readonly TextBlock caption = new() { FontSize = 11, Foreground = TextBrush, HorizontalAlignment = HorizontalAlignment.Center };
        private readonly StackPanel legend = new() { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(28, 0, 0, 0) };

        /// <summary>Creates the control with an empty ring.</summary>
        public DonutChart()
        {
            Content = BuildLayout();
            SetRows(Array.Empty<HistoryRow>(), DateTime.UtcNow);
        }

        /// <summary>Shows the given rows. Call it when the history changed.</summary>
        /// <param name="rows">History rows of the last 24 hours.</param>
        /// <param name="nowUtc">The current time, Kind Utc.</param>
        public void SetRows(IReadOnlyList<HistoryRow> rows, DateTime nowUtc)
        {
            DonutLayout layout = DonutGeometry.Build(rows, nowUtc);
            DrawRing(layout);
            total.Text = layout.HasData ? layout.TotalKwh.ToString("F2", CultureInfo.InvariantCulture) : "No data";
            caption.Text = layout.HasData ? "kWh in 24 h" : "";
            DrawLegend(layout);
        }

        /// <summary>Builds the fixed parts: the title, the ring with the centre text, and the legend column.</summary>
        /// <returns>The root element.</returns>
        private UIElement BuildLayout()
        {
            var centre = new StackPanel { HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center };
            centre.Children.Add(total);
            centre.Children.Add(caption);
            var ringHost = new Grid { Width = RingSize, Height = RingSize };
            ringHost.Children.Add(ring);
            ringHost.Children.Add(centre);
            var row = new StackPanel { Orientation = Orientation.Horizontal };
            row.Children.Add(ringHost);
            row.Children.Add(legend);
            var title = new TextBlock { Text = "Energy by state (24 h)", FontSize = 16, FontWeight = FontWeights.SemiBold, Margin = new Thickness(0, 0, 0, 12) };
            var root = new StackPanel { Margin = new Thickness(20) };
            root.Children.Add(title);
            root.Children.Add(row);
            return root;
        }

        /// <summary>Clears the ring and draws one arc for each slice with energy, or the empty ring.</summary>
        /// <param name="layout">The donut layout.</param>
        private void DrawRing(DonutLayout layout)
        {
            ring.Children.Clear();
            if (!layout.HasData) ring.Children.Add(FullRing(EmptyRingBrush));
            foreach (DonutSlice slice in layout.Slices) AddSlice(slice);
        }

        /// <summary>Adds the arc of one slice. A slice without energy adds nothing.</summary>
        /// <param name="slice">The slice.</param>
        private void AddSlice(DonutSlice slice)
        {
            if (slice.SweepAngle <= 0) return;
            ring.Children.Add(slice.SweepAngle >= FullCircleDegrees ? FullRing(BrushOf(slice.State)) : Arc(slice));
        }

        /// <summary>Makes a full circle outline (a one-state ring, or the empty ring).</summary>
        /// <param name="brush">The colour of the ring.</param>
        /// <returns>The ellipse. An arc of 360 degrees has no visible start and end for WPF, so an ellipse is used instead.</returns>
        private static Ellipse FullRing(Brush brush)
        {
            double size = RingSize - RingThickness;
            var ellipse = new Ellipse { Width = size, Height = size, Stroke = brush, StrokeThickness = RingThickness };
            Canvas.SetLeft(ellipse, RingThickness / 2);
            Canvas.SetTop(ellipse, RingThickness / 2);
            return ellipse;
        }

        /// <summary>Makes the arc of a slice as a stroked Path.</summary>
        /// <param name="slice">A slice with a sweep above 0 and below a full circle.</param>
        /// <returns>The path.</returns>
        private static Path Arc(DonutSlice slice)
        {
            double centre = RingSize / 2;
            double radius = (RingSize - RingThickness) / 2;
            (double x0, double y0) = DonutGeometry.PointOnCircle(centre, centre, radius, slice.StartAngle);
            (double x1, double y1) = DonutGeometry.PointOnCircle(centre, centre, radius, slice.StartAngle + slice.SweepAngle);
            var segment = new ArcSegment(new Point(x1, y1), new Size(radius, radius), 0, slice.SweepAngle > 180, SweepDirection.Clockwise, true);
            var figure = new PathFigure { StartPoint = new Point(x0, y0) };
            figure.Segments.Add(segment);
            var geometry = new PathGeometry();
            geometry.Figures.Add(figure);
            return new Path { Data = geometry, Stroke = BrushOf(slice.State), StrokeThickness = RingThickness };
        }

        /// <summary>Clears the legend and adds one row for each state.</summary>
        /// <param name="layout">The donut layout.</param>
        private void DrawLegend(DonutLayout layout)
        {
            legend.Children.Clear();
            foreach (DonutSlice slice in layout.Slices) legend.Children.Add(LegendRow(slice, layout.HasData));
        }

        /// <summary>Makes one legend row: a colour square, the state word, its kWh and its percent.</summary>
        /// <param name="slice">The slice of the state.</param>
        /// <param name="hasData">False when the values are shown as dashes.</param>
        /// <returns>The row.</returns>
        private static UIElement LegendRow(DonutSlice slice, bool hasData)
        {
            string kwh = hasData ? slice.Kwh.ToString("F2", CultureInfo.InvariantCulture) + " kWh" : "-";
            string percent = hasData ? slice.Percent.ToString("F1", CultureInfo.InvariantCulture) + " %" : "-";
            var row = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 4, 0, 4) };
            row.Children.Add(new Rectangle { Width = 12, Height = 12, Fill = BrushOf(slice.State), Margin = new Thickness(0, 0, 8, 0) });
            row.Children.Add(Text(StateWord(slice.State), 64, TextAlignment.Left));
            row.Children.Add(Text(kwh, 84, TextAlignment.Left));
            row.Children.Add(Text(percent, 60, TextAlignment.Left));
            return row;
        }

        /// <summary>Makes a legend text with a fixed width, so the columns line up.</summary>
        /// <param name="text">The text.</param>
        /// <param name="width">The width in device independent pixels.</param>
        /// <param name="align">The alignment inside the width.</param>
        /// <returns>The text block.</returns>
        private static TextBlock Text(string text, double width, TextAlignment align)
            => new() { Text = text, Width = width, TextAlignment = align, FontSize = 13 };

        private static string StateWord(int state) => state == 2 ? "Active" : state == 1 ? "Idle" : "Unused";

        private static Brush BrushOf(int state) => state == 2 ? ActiveBrush : state == 1 ? IdleBrush : UnusedBrush;

        /// <summary>Makes a frozen brush from a hex colour such as "#3FA34D".</summary>
        /// <param name="hex">The colour.</param>
        /// <returns>The frozen brush.</returns>
        private static Brush Make(string hex)
        {
            var brush = new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex));
            brush.Freeze();
            return brush;
        }
    }
}
