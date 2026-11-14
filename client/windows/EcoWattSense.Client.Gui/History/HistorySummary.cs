// HistorySummary.cs: the two numbers of the information panel: power now and energy today.
// Who uses it: the client window, with the rows from HistoryReader.
// Pure code with no WPF, so a test script can compile and run it.
#nullable enable
using System;
using System.Collections.Generic;
using System.Linq;

namespace EcoWattSense.Client.Gui.History
{
    /// <summary>
    /// Sums and picks values from history rows.
    /// </summary>
    /// <summary>Power now: the watts and whether they are the measured part only.</summary>
    /// <param name="Watts">Device W, else measured W of the newest fresh row, or null.</param>
    /// <param name="MeasuredOnly">True when device W was empty and Watts is the measured W.</param>
    public readonly record struct PowerReading(double? Watts, bool MeasuredOnly);

    /// <summary>Energy today: the kWh and whether any row was counted from the measured part only.</summary>
    /// <param name="Kwh">The energy, or null when no row of today has a value.</param>
    /// <param name="IncludesMeasuredOnly">True when at least one counted row had no device W.</param>
    public readonly record struct TodayEnergy(double? Kwh, bool IncludesMeasuredOnly);

    public static class HistorySummary
    {
        /// <summary>A last row older than this is stale: the service is not writing.</summary>
        public static readonly TimeSpan FreshLimit = TimeSpan.FromMinutes(3);

        /// <summary>
        /// Power now: device W of the newest row, when that row ended less than 3 minutes ago.
        /// </summary>
        /// <param name="rows">Rows in any order.</param>
        /// <param name="nowUtc">The current time, Kind Utc.</param>
        /// <returns>Watts, or null when there is no row, the newest row is stale, or its value is empty.</returns>
        public static double? PowerNow(IReadOnlyList<HistoryRow> rows, DateTime nowUtc)
        {
            HistoryRow? last = rows.MaxBy(Ended);
            if (last == null) return null;
            if (nowUtc - last.EndUtc >= FreshLimit) return null;
            return last.DeviceW;
        }

        /// <summary>
        /// Energy today in kWh: the sum of device W times seconds over the rows that ended
        /// on today's day of the given time zone. A row belongs to the day it ended on.
        /// A row with no device W counts with its measured W. Empty values are left out.
        /// </summary>
        /// <param name="rows">Rows in any order.</param>
        /// <param name="nowUtc">The current time, Kind Utc.</param>
        /// <param name="zone">The local time zone (TimeZoneInfo.Local in the app).</param>
        /// <returns>kWh, or null when no row of today has a value.</returns>
        public static double? EnergyTodayKwh(IReadOnlyList<HistoryRow> rows, DateTime nowUtc, TimeZoneInfo zone)
        {
            return EnergyToday(rows, nowUtc, zone).Kwh;
        }

        /// <summary>
        /// Power now with the source of the value: device W of the newest row, else its measured W (SPEC 9).
        /// </summary>
        /// <param name="rows">Rows in any order.</param>
        /// <param name="nowUtc">The current time, Kind Utc.</param>
        /// <returns>Unknown (null watts) when there is no row, the newest row is stale, or both values are empty.</returns>
        public static PowerReading PowerNowReading(IReadOnlyList<HistoryRow> rows, DateTime nowUtc)
        {
            HistoryRow? last = rows.MaxBy(Ended);
            if (last == null || nowUtc - last.EndUtc >= FreshLimit) return new PowerReading(null, false);
            if (last.DeviceW != null) return new PowerReading(last.DeviceW, false);
            return new PowerReading(last.MeasuredW, last.MeasuredW != null);
        }

        /// <summary>
        /// Energy today like EnergyTodayKwh, but a row with no device W counts with its measured W.
        /// </summary>
        /// <param name="rows">Rows in any order.</param>
        /// <param name="nowUtc">The current time, Kind Utc.</param>
        /// <param name="zone">The local time zone.</param>
        /// <returns>The kWh and whether a measured-only row was part of it.</returns>
        public static TodayEnergy EnergyToday(IReadOnlyList<HistoryRow> rows, DateTime nowUtc, TimeZoneInfo zone)
        {
            var tally = new DayTally(TimeZoneInfo.ConvertTimeFromUtc(nowUtc, zone).Date, zone);
            foreach (HistoryRow row in rows) tally.Add(row);
            return new TodayEnergy(tally.Kwh(), tally.MeasuredOnlyRows > 0);
        }

        private static DateTime Ended(HistoryRow row) => row.EndUtc;

        /// <summary>Collects watt-seconds of one local day.</summary>
        private sealed class DayTally
        {
            private readonly DateTime day;
            private readonly TimeZoneInfo zone;
            private double wattSeconds;
            private int used;

            /// <summary>Number of added rows that had no device W and used the measured W.</summary>
            public int MeasuredOnlyRows { get; private set; }

            public DayTally(DateTime day, TimeZoneInfo zone)
            {
                this.day = day;
                this.zone = zone;
            }

            /// <summary>Adds the row when it ended today and has a device W or measured W value.</summary>
            public void Add(HistoryRow row)
            {
                double? watts = row.DeviceW ?? row.MeasuredW;
                if (watts == null) return;
                if (TimeZoneInfo.ConvertTimeFromUtc(row.EndUtc, zone).Date != day) return;
                wattSeconds += watts.Value * row.Seconds;
                used++;
                MeasuredOnlyRows += row.DeviceW == null ? 1 : 0;
            }

            /// <summary>Gives kWh, or null when nothing was added.</summary>
            public double? Kwh() => used == 0 ? null : wattSeconds / 3600000.0;
        }
    }
}
