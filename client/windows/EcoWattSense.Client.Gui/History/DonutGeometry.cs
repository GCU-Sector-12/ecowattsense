// DonutGeometry.cs: the numbers behind the "Energy by state (24 h)" donut (SPEC 9).
// Who uses it: DonutChart.cs draws the arcs from this layout; Test-DonutGeometry.ps1 checks it.
// It is pure: history rows in, energy per state and arc angles out. No WPF types, so a script can test it.
// Energy of a row = device W x seconds of the row. Rows with an empty device W or an empty state are left out,
// and so are rows that ended 24 hours ago or earlier.
#nullable enable
using System;
using System.Collections.Generic;
using System.Linq;

namespace EcoWattSense.Client.Gui.History
{
    /// <summary>The colours of the three states, the same as the state band of the 24 hour chart (HistoryChart.cs).</summary>
    public static class StateColours
    {
        /// <summary>Active, as a hex colour.</summary>
        public const string Active = "#3FA34D";
        /// <summary>Idle, as a hex colour.</summary>
        public const string Idle = "#E5B53B";
        /// <summary>Unused, as a hex colour.</summary>
        public const string Unused = "#B8C0CC";
    }

    /// <summary>One state of the donut.</summary>
    /// <param name="State">The state number: 2 Active, 1 Idle, 0 Unused.</param>
    /// <param name="Kwh">The energy of the state in kilowatt hours.</param>
    /// <param name="Percent">The share of the total in percent, 0 to 100.</param>
    /// <param name="StartAngle">Where the arc starts, in degrees clockwise from the top, 0 to 360.</param>
    /// <param name="SweepAngle">The length of the arc in degrees. 0 when the state has no energy.</param>
    public readonly record struct DonutSlice(int State, double Kwh, double Percent, double StartAngle, double SweepAngle);

    /// <summary>The donut: three slices in the order Active, Idle, Unused, and the total.</summary>
    /// <param name="HasData">False when no row counted, or the total energy is zero.</param>
    /// <param name="TotalKwh">The total energy in kilowatt hours.</param>
    /// <param name="Slices">Always three slices. The sweep angles of the slices add up to exactly 360 when HasData is true.</param>
    public sealed record DonutLayout(bool HasData, double TotalKwh, IReadOnlyList<DonutSlice> Slices);

    /// <summary>Builds the donut layout.</summary>
    public static class DonutGeometry
    {
        private const double JouleToKwh = 1.0 / 3600000.0;
        private static readonly int[] StateOrder = { 2, 1, 0 };

        /// <summary>Builds the layout from the history rows.</summary>
        /// <param name="rows">History rows in any order.</param>
        /// <param name="nowUtc">The current time, Kind Utc. Rows that ended more than 24 hours before it are left out.</param>
        /// <returns>The layout. Without usable rows, HasData is false and every slice is empty.</returns>
        public static DonutLayout Build(IReadOnlyList<HistoryRow> rows, DateTime nowUtc)
        {
            DateTime start = nowUtc.AddHours(-24);
            double[] kwh = new double[3];
            foreach (HistoryRow row in rows.Where(row => Counts(row, start))) kwh[Index(row.State!.Value)] += Energy(row);
            double total = kwh.Sum();
            return total > 0 ? WithData(kwh, total) : Empty();
        }

        /// <summary>Gives a point on a circle.</summary>
        /// <param name="centreX">The x of the centre.</param>
        /// <param name="centreY">The y of the centre.</param>
        /// <param name="radius">The radius.</param>
        /// <param name="degrees">The angle in degrees, clockwise from the top.</param>
        /// <returns>The x and y of the point (y grows downwards, as in WPF).</returns>
        public static (double X, double Y) PointOnCircle(double centreX, double centreY, double radius, double degrees)
        {
            double radians = degrees * Math.PI / 180.0;
            return (centreX + radius * Math.Sin(radians), centreY - radius * Math.Cos(radians));
        }

        /// <summary>Tells whether a row takes part in the donut.</summary>
        /// <param name="row">A history row.</param>
        /// <param name="start">The earliest end time that counts (exclusive).</param>
        /// <returns>True when the row has a device W that is a finite number of 0 or more, a state from 0 to 2, and ended after start.</returns>
        private static bool Counts(HistoryRow row, DateTime start)
            => row.EndUtc > start
               && row.State is >= 0 and <= 2
               && row.DeviceW is { } watts && double.IsFinite(watts) && watts >= 0;

        /// <summary>Gives the position of a state in the slice list.</summary>
        /// <param name="state">The state number from 0 to 2.</param>
        /// <returns>0 for Active, 1 for Idle, 2 for Unused.</returns>
        private static int Index(int state) => 2 - state;

        /// <summary>Gives the energy of one row.</summary>
        /// <param name="row">A row that passed <see cref="Counts"/>.</param>
        /// <returns>The energy in kilowatt hours.</returns>
        private static double Energy(HistoryRow row) => row.DeviceW!.Value * row.Seconds * JouleToKwh;

        /// <summary>Builds the layout for a total above zero.</summary>
        /// <param name="kwh">The energy per slice.</param>
        /// <param name="total">The sum of kwh, above zero.</param>
        /// <returns>The layout. The last slice with energy ends at exactly 360 degrees, so rounding never leaves a gap.</returns>
        private static DonutLayout WithData(double[] kwh, double total)
        {
            int last = Array.FindLastIndex(kwh, value => value > 0);
            var slices = new List<DonutSlice>();
            double before = 0;
            for (int i = 0; i < kwh.Length; i++)
            {
                double startAngle = i > last ? 360 : before / total * 360;
                double endAngle = i >= last ? 360 : (before + kwh[i]) / total * 360;
                slices.Add(new DonutSlice(StateOrder[i], kwh[i], kwh[i] / total * 100, startAngle, endAngle - startAngle));
                before += kwh[i];
            }
            return new DonutLayout(true, total, slices);
        }

        /// <summary>Builds the layout for no data.</summary>
        /// <returns>A layout with HasData false, total 0 and three empty slices.</returns>
        private static DonutLayout Empty()
        {
            var slices = StateOrder.Select(state => new DonutSlice(state, 0, 0, 0, 0)).ToList();
            return new DonutLayout(false, 0, slices);
        }
    }
}
