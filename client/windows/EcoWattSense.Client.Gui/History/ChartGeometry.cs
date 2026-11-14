// ChartGeometry.cs: the pure geometry of the 24 hour chart: rows in, segments and rectangles out.
// Who uses it: HistoryChart (draws the result with WPF shapes) and Test-HistoryChart.ps1.
// It has no WPF types, so a test can compile and run it without a window.
// Coordinates: X is in minute slots (0 to 1440, the left edge of the window to the right edge),
// Y is in watts. HistoryChart scales both to pixels.
#nullable enable
using System;
using System.Collections.Generic;
using System.Globalization;

namespace EcoWattSense.Client.Gui.History
{
    /// <summary>A point of the line: X in slots, Y in watts.</summary>
    public readonly record struct ChartPoint(double X, double Y);

    /// <summary>A rectangle band: starts at X slots, is Width slots wide. State is 0, 1 or 2 for the state band (1 for battery).</summary>
    public readonly record struct ChartRect(double X, double Width, int State);

    /// <summary>A label on the time axis at X slots.</summary>
    public readonly record struct ChartTick(double X, string Label);

    /// <summary>Everything HistoryChart needs to draw one frame of data.</summary>
    public sealed class ChartLayout
    {
        /// <summary>Line pieces. A missing minute or an empty value ends a piece.</summary>
        public List<List<ChartPoint>> Segments { get; } = new();

        /// <summary>Runs of minutes on battery.</summary>
        public List<ChartRect> BatteryRects { get; } = new();

        /// <summary>Runs of the same state (State 0 Unused, 1 Idle, 2 Active).</summary>
        public List<ChartRect> StateRects { get; } = new();

        /// <summary>Top of the watt axis. The axis starts at 0.</summary>
        public double AxisMax { get; set; }

        /// <summary>Watt values of the axis labels: 0, half of the maximum, the maximum.</summary>
        public List<double> WattTicks { get; } = new();

        /// <summary>Hour labels every 3 hours of local time.</summary>
        public List<ChartTick> HourTicks { get; } = new();
    }

    /// <summary>
    /// Builds a ChartLayout from history rows.
    /// </summary>
    public static class ChartGeometry
    {
        /// <summary>Number of minute slots in the window (24 hours).</summary>
        public const int SlotCount = 1440;

        /// <summary>Axis maximum used when no row has a value.</summary>
        public const double DefaultAxisMax = 10;

        /// <summary>
        /// Builds the layout. The window is the 1440 minutes ending with the minute of nowUtc.
        /// A row goes into the slot of its end time; of two rows in one slot the later in the list wins.
        /// </summary>
        /// <param name="rows">Rows oldest first (HistoryReader sorts them).</param>
        /// <param name="nowUtc">The current time, Kind Utc.</param>
        /// <param name="zone">Time zone for the hour labels.</param>
        /// <returns>The layout. Never null; empty when there are no rows.</returns>
        public static ChartLayout Build(IReadOnlyList<HistoryRow> rows, DateTime nowUtc, TimeZoneInfo zone)
        {
            DateTime start = WindowStart(nowUtc);
            HistoryRow?[] slots = new HistoryRow?[SlotCount];
            foreach (HistoryRow row in rows) Place(slots, start, row);

            var layout = new ChartLayout();
            var line = new LineBuilder(layout.Segments);
            var battery = new RunBuilder(layout.BatteryRects);
            var states = new RunBuilder(layout.StateRects);
            double max = 0;
            for (int i = 0; i < SlotCount; i++)
            {
                line.Feed(i, slots[i]?.DeviceW);
                battery.Feed(i, BatteryKey(slots[i]));
                states.Feed(i, StateKey(slots[i]));
                max = Math.Max(max, slots[i]?.DeviceW ?? 0);
            }
            line.Finish();
            battery.Finish();
            states.Finish();

            layout.AxisMax = NiceMax(max);
            layout.WattTicks.AddRange(new[] { 0, layout.AxisMax / 2, layout.AxisMax });
            AddHourTicks(layout.HourTicks, start, zone);
            return layout;
        }

        /// <summary>First minute of the window: the minute of nowUtc minus 1439 minutes.</summary>
        public static DateTime WindowStart(DateTime nowUtc)
        {
            long minuteTicks = nowUtc.Ticks - nowUtc.Ticks % TimeSpan.TicksPerMinute;
            return new DateTime(minuteTicks, DateTimeKind.Utc).AddMinutes(-(SlotCount - 1));
        }

        /// <summary>
        /// Rounds a maximum up to 1, 2, 5 or 10 times a power of ten, so the labels stay round.
        /// </summary>
        /// <param name="max">Largest watt value; zero or less gives DefaultAxisMax.</param>
        public static double NiceMax(double max)
        {
            if (max <= 0) return DefaultAxisMax;
            double magnitude = Math.Pow(10, Math.Floor(Math.Log10(max)));
            double n = max / magnitude;
            return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * magnitude;
        }

        /// <summary>Puts a row into its slot; a row outside the window is dropped.</summary>
        private static void Place(HistoryRow?[] slots, DateTime start, HistoryRow row)
        {
            int index = (int)Math.Floor((row.EndUtc - start).TotalMinutes);
            if (index < 0 || index >= SlotCount) return;
            slots[index] = row;
        }

        private static int BatteryKey(HistoryRow? row) => row != null && row.OnBattery ? 1 : -1;

        private static int StateKey(HistoryRow? row) => row?.State ?? -1;

        /// <summary>Adds a label for each full local hour whose hour number divides by 3.</summary>
        private static void AddHourTicks(List<ChartTick> ticks, DateTime start, TimeZoneInfo zone)
        {
            int toFullHour = (60 - TimeZoneInfo.ConvertTimeFromUtc(start, zone).Minute) % 60;
            for (int hour = 0; hour <= 24; hour++) AddHourTick(ticks, start, zone, toFullHour + 60 * hour);
        }

        /// <summary>Adds one label when the slot is inside the window and its local hour is 0, 3, 6 ... 21.</summary>
        private static void AddHourTick(List<ChartTick> ticks, DateTime start, TimeZoneInfo zone, int slot)
        {
            if (slot >= SlotCount) return;
            DateTime local = TimeZoneInfo.ConvertTimeFromUtc(start.AddMinutes(slot), zone);
            if (local.Hour % 3 != 0) return;
            ticks.Add(new ChartTick(slot, local.ToString("HH:mm", CultureInfo.InvariantCulture)));
        }

        /// <summary>Joins neighbouring slots with the same key (0 or more) into rectangles; key -1 means none.</summary>
        private sealed class RunBuilder
        {
            private readonly List<ChartRect> target;
            private int key = -1;
            private int begin;

            public RunBuilder(List<ChartRect> target) => this.target = target;

            /// <summary>Takes the key of the next slot. A new key closes the run before it.</summary>
            public void Feed(int slot, int slotKey)
            {
                if (slotKey == key) return;
                Close(slot);
                key = slotKey;
                begin = slot;
            }

            /// <summary>Closes the last run at the end of the window.</summary>
            public void Finish() => Close(SlotCount);

            private void Close(int end)
            {
                if (key < 0) return;
                target.Add(new ChartRect(begin, end - begin, key));
            }
        }

        /// <summary>Joins neighbouring slots with a value into line pieces, drawn through the slot centres.</summary>
        private sealed class LineBuilder
        {
            private readonly List<List<ChartPoint>> target;
            private List<ChartPoint> points = new();

            public LineBuilder(List<List<ChartPoint>> target) => this.target = target;

            /// <summary>Takes the watt value of the next slot; null ends the current piece.</summary>
            public void Feed(int slot, double? watts)
            {
                if (watts == null) { Close(); return; }
                points.Add(new ChartPoint(slot + 0.5, watts.Value));
            }

            /// <summary>Ends the last piece at the end of the window.</summary>
            public void Finish() => Close();

            /// <summary>Stores the piece. A lone minute becomes a short flat line across its slot, so it stays visible.</summary>
            private void Close()
            {
                if (points.Count == 0) return;
                if (points.Count == 1) points = new List<ChartPoint> { new(points[0].X - 0.5, points[0].Y), new(points[0].X + 0.5, points[0].Y) };
                target.Add(points);
                points = new List<ChartPoint>();
            }
        }
    }
}
