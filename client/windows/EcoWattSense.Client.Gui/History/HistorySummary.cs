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
        /// Empty values are left out.
        /// </summary>
        /// <param name="rows">Rows in any order.</param>
        /// <param name="nowUtc">The current time, Kind Utc.</param>
        /// <param name="zone">The local time zone (TimeZoneInfo.Local in the app).</param>
        /// <returns>kWh, or null when no row of today has a value.</returns>
        public static double? EnergyTodayKwh(IReadOnlyList<HistoryRow> rows, DateTime nowUtc, TimeZoneInfo zone)
        {
            var tally = new DayTally(TimeZoneInfo.ConvertTimeFromUtc(nowUtc, zone).Date, zone);
            foreach (HistoryRow row in rows) tally.Add(row);
            return tally.Kwh();
        }

        private static DateTime Ended(HistoryRow row) => row.EndUtc;

        /// <summary>Collects watt-seconds of one local day.</summary>
        private sealed class DayTally
        {
            private readonly DateTime day;
            private readonly TimeZoneInfo zone;
            private double wattSeconds;
            private int used;

            public DayTally(DateTime day, TimeZoneInfo zone)
            {
                this.day = day;
                this.zone = zone;
            }

            /// <summary>Adds the row when it ended today and has a device W value.</summary>
            public void Add(HistoryRow row)
            {
                if (row.DeviceW == null) return;
                if (TimeZoneInfo.ConvertTimeFromUtc(row.EndUtc, zone).Date != day) return;
                wattSeconds += row.DeviceW.Value * row.Seconds;
                used++;
            }

            /// <summary>Gives kWh, or null when nothing was added.</summary>
            public double? Kwh() => used == 0 ? null : wattSeconds / 3600000.0;
        }
    }
}
