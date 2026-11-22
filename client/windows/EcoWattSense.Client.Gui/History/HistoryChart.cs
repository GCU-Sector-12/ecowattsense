// HistoryChart.cs: the 24 hour chart control of the client window (SPEC section 9).
// Who uses it: MainWindow puts one into the Border named HistoryHost and calls SetRows after each history change.
// It draws a line of device W, a dashed line of measured W where device W is empty, shading for time on battery, a band of the state per minute,
// hour labels, a watt axis and a legend. The shapes are built once per data change or size change
// (no timer, no frame-by-frame drawing); WPF renders them on the GPU.
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
using Point = System.Windows.Point;
using Rectangle = System.Windows.Shapes.Rectangle;
using ColorConverter = System.Windows.Media.ColorConverter;

namespace EcoWattSense.Client.Gui.History
{
    /// <summary>
    /// A 24 hour power chart. Call SetRows to show new data.
    /// </summary>
    public sealed class HistoryChart : UserControl
    {
        private const double LeftMargin = 46;
        private const double RightMargin = 8;
        private const double TopMargin = 8;
        private const double BandHeight = 10;
        private const double HourLabelHeight = 16;
        private const double LegendHeight = 18;

        private static readonly Brush LineBrush = Make("#285E50");
        private static readonly Brush BatteryBrush = Make("#FBE3CC");
        private static readonly Brush GridBrush = Make("#E3E8EE");
        private static readonly Brush TextBrush = Make("#728095");
        private static readonly Brush ActiveBrush = Make("#3FA34D");
        private static readonly Brush IdleBrush = Make("#E5B53B");
        private static readonly Brush UnusedBrush = Make("#B8C0CC");

        private readonly Canvas canvas = new() { ClipToBounds = true };
        private IReadOnlyList<HistoryRow> rows = Array.Empty<HistoryRow>();
        private DateTime nowUtc = DateTime.UtcNow;
        private double plotWidth;
        private double plotHeight;
        private double plotBottom;
        private double axisMax = 1;

        /// <summary>Creates an empty chart.</summary>
        public HistoryChart()
        {
            Content = canvas;
            SizeChanged += OnSizeChanged;
        }

        /// <summary>
        /// Shows the given rows. Call it when the history changed.
        /// </summary>
        /// <param name="rows">Rows of the last 24 hours, oldest first.</param>
        /// <param name="nowUtc">The current time, Kind Utc; it is the right edge of the chart.</param>
        public void SetRows(IReadOnlyList<HistoryRow> rows, DateTime nowUtc)
        {
            this.rows = rows;
            this.nowUtc = nowUtc;
            Rebuild();
        }

        private void OnSizeChanged(object sender, SizeChangedEventArgs e) => Rebuild();

        /// <summary>Clears the canvas and builds all shapes again. A window too small to read gets nothing.</summary>
        private void Rebuild()
        {
            canvas.Children.Clear();
            if (ActualWidth < 200 || ActualHeight < 120) return;
            ChartLayout layout = ChartGeometry.Build(rows, nowUtc, TimeZoneInfo.Local);
            plotWidth = ActualWidth - LeftMargin - RightMargin;
            plotBottom = ActualHeight - LegendHeight - HourLabelHeight - BandHeight - 10;
            plotHeight = plotBottom - TopMargin;
            axisMax = layout.AxisMax;
            DrawBattery(layout);
            DrawGrid(layout);
            DrawLine(layout);
            DrawBand(layout);
            DrawHourLabels(layout);
            DrawLegend(layout.MeasuredSegments.Count > 0);
        }

        private double X(double slot) => LeftMargin + slot / ChartGeometry.SlotCount * plotWidth;

        private double Y(double watts) => plotBottom - watts / axisMax * plotHeight;

        private void DrawBattery(ChartLayout layout)
        {
            foreach (ChartRect r in layout.BatteryRects) AddRect(X(r.X), TopMargin, r.Width / ChartGeometry.SlotCount * plotWidth, plotHeight, BatteryBrush);
        }

        private void DrawGrid(ChartLayout layout)
        {
            foreach (double watts in layout.WattTicks) AddWattTick(watts);
        }

        /// <summary>Draws one horizontal grid line and its right-aligned label.</summary>
        private void AddWattTick(double watts)
        {
            double y = Y(watts);
            canvas.Children.Add(new Line { X1 = LeftMargin, X2 = LeftMargin + plotWidth, Y1 = y, Y2 = y, Stroke = GridBrush, StrokeThickness = 1 });
            AddText(watts.ToString("0.#", CultureInfo.CurrentCulture) + " W", 0, y - 8, LeftMargin - 6, TextAlignment.Right);
        }

        private void DrawLine(ChartLayout layout)
        {
            foreach (List<ChartPoint> piece in layout.Segments) AddPolyline(piece);
            foreach (List<ChartPoint> piece in layout.MeasuredSegments) AddPolyline(piece, true);
        }

        /// <summary>Draws one line piece; the measured part is dashed in the same colour.</summary>
        private void AddPolyline(List<ChartPoint> piece, bool dashed = false)
        {
            var line = new Polyline { Stroke = LineBrush, StrokeThickness = 1.5, StrokeLineJoin = PenLineJoin.Round };
            if (dashed) line.StrokeDashArray = new DoubleCollection { 3, 2 };
            foreach (ChartPoint p in piece) line.Points.Add(new Point(X(p.X), Y(p.Y)));
            canvas.Children.Add(line);
        }

        private void DrawBand(ChartLayout layout)
        {
            foreach (ChartRect r in layout.StateRects) AddRect(X(r.X), plotBottom + 4, r.Width / ChartGeometry.SlotCount * plotWidth, BandHeight, StateBrush(r.State));
        }

        private static Brush StateBrush(int state) => state == 2 ? ActiveBrush : state == 1 ? IdleBrush : UnusedBrush;

        private void DrawHourLabels(ChartLayout layout)
        {
            foreach (ChartTick t in layout.HourTicks) AddText(t.Label, X(t.X) - 22, plotBottom + 4 + BandHeight + 1, 44, TextAlignment.Center);
        }

        /// <summary>Draws the legend: Battery, Active, Idle, Unused, and the dashed measured part when it is drawn.</summary>
        private void DrawLegend(bool withMeasured)
        {
            double y = ActualHeight - LegendHeight + 2;
            AddLegendItem(0, y, BatteryBrush, "Battery");
            AddLegendItem(1, y, ActiveBrush, "Active");
            AddLegendItem(2, y, IdleBrush, "Idle");
            AddLegendItem(3, y, UnusedBrush, "Unused");
            if (withMeasured) AddLegendItem(4, y, LineBrush, "Measured part only (dashed)", 160);
        }

        private void AddLegendItem(int index, double y, Brush brush, string word, double textWidth = 70)
        {
            double x = LeftMargin + index * 86;
            AddRect(x, y + 3, 10, 10, brush);
            AddText(word, x + 14, y, textWidth, TextAlignment.Left);
        }

        private void AddRect(double x, double y, double width, double height, Brush brush)
        {
            var rect = new Rectangle { Width = Math.Max(width, 0), Height = height, Fill = brush };
            Canvas.SetLeft(rect, x);
            Canvas.SetTop(rect, y);
            canvas.Children.Add(rect);
        }

        private void AddText(string text, double x, double y, double width, TextAlignment align)
        {
            var block = new TextBlock { Text = text, Width = width, TextAlignment = align, FontSize = 11, Foreground = TextBrush };
            Canvas.SetLeft(block, x);
            Canvas.SetTop(block, y);
            canvas.Children.Add(block);
        }

        /// <summary>Makes a frozen brush from a hex colour such as "#285E50".</summary>
        private static Brush Make(string hex)
        {
            var brush = new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex));
            brush.Freeze();
            return brush;
        }
    }
}
