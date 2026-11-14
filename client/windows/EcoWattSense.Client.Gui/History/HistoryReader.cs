// HistoryReader.cs: reads the service history files of the last 24 hours (SPEC section 7).
// Who uses it: the client window, which passes the history folder and the current UTC time.
// The service appends to these files while we read, so they are opened with shared read and write.
// The files are untrusted: every value is range-checked, and a bad row is skipped and counted.
#nullable enable
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

namespace EcoWattSense.Client.Gui.History
{
    /// <summary>
    /// What one call of HistoryReader.Read found.
    /// </summary>
    public sealed class HistoryReadResult
    {
        private readonly List<HistoryRow> rows = new();

        /// <summary>Rows of the last 24 hours, oldest first.</summary>
        public IReadOnlyList<HistoryRow> Rows => rows;

        /// <summary>Rows that could not be parsed or had a value out of range.</summary>
        public int BadRows { get; internal set; }

        /// <summary>Files opened and found to have the right header.</summary>
        public int FilesRead { get; internal set; }

        /// <summary>Files that exist but could not be read, or have a wrong header.</summary>
        public int FilesUnreadable { get; internal set; }

        internal void Add(HistoryRow row) => rows.Add(row);

        internal void SortRows() => rows.Sort(CompareByEnd);

        private static int CompareByEnd(HistoryRow a, HistoryRow b) => a.EndUtc.CompareTo(b.EndUtc);
    }

    /// <summary>
    /// Reads and checks the history files.
    /// </summary>
    public static class HistoryReader
    {
        /// <summary>The first line every history file must have.</summary>
        public const string Header = "v1,ts_utc,seconds,on_battery,state,idle_s,cpu_pct,device_w,measured_w,battery_w,level";

        private static readonly Regex LevelPattern = new("^L[0-9]$", RegexOptions.CultureInvariant);

        /// <summary>
        /// Reads the files that can hold rows of the last 24 hours (the UTC days of
        /// nowUtc minus 24 hours and of nowUtc), and keeps the rows that ended in that time.
        /// A missing folder or file gives no rows and no error.
        /// </summary>
        /// <param name="folder">The history folder, for example C:\ProgramData\EcoWattSense\history.</param>
        /// <param name="nowUtc">The current time, Kind Utc.</param>
        /// <returns>The rows and the counts of bad rows and unreadable files. Never throws for file problems.</returns>
        public static HistoryReadResult Read(string folder, DateTime nowUtc)
        {
            var result = new HistoryReadResult();
            DateTime cutoff = nowUtc.AddHours(-24);
            ReadFile(FilePath(folder, cutoff), cutoff, result);
            ReadFile(FilePath(folder, nowUtc), cutoff, result);
            result.SortRows();
            return result;
        }

        /// <summary>
        /// Parses one data row. Public so that tests can reach it.
        /// </summary>
        /// <param name="line">One line without the line break.</param>
        /// <returns>The row, or null when the line is bad (wrong field count, unparsable or out of range).</returns>
        public static HistoryRow? ParseRow(string line)
        {
            string[] f = line.Split(',');
            if (f.Length != 11 || f[0] != "v1") return null;
            if (!TryTime(f[1], out DateTime end)) return null;
            if (!TrySeconds(f[2], out int seconds)) return null;
            if (f[3] != "0" && f[3] != "1") return null;
            if (!TryState(f[4], out int? state)) return null;
            if (!TryOptional(f[5], 0, 31536000, out double? idle)) return null;
            if (!TryOptional(f[6], 0, 100, out double? cpu)) return null;
            if (!TryOptional(f[7], 0, 10000, out double? device)) return null;
            if (!TryOptional(f[8], 0, 10000, out double? measured)) return null;
            if (!TryOptional(f[9], -10000, 10000, out double? battery)) return null;
            if (f[10] != "" && !LevelPattern.IsMatch(f[10])) return null;
            string? level = f[10] == "" ? null : f[10];
            return new HistoryRow(end, seconds, f[3] == "1", state, idle, cpu, device, measured, battery, level);
        }

        private static string FilePath(string folder, DateTime utcDay) =>
            Path.Combine(folder, utcDay.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture) + ".csv");

        /// <summary>Reads one file into the result. Missing file: nothing. Unreadable or wrong header: counted.</summary>
        private static void ReadFile(string path, DateTime cutoff, HistoryReadResult result)
        {
            if (!File.Exists(path)) return;
            string[]? lines = TryReadLines(path);
            if (lines == null) { result.FilesUnreadable++; return; }
            if (lines.Length > 0 && lines[0].TrimEnd('\r') != Header) { result.FilesUnreadable++; return; }
            result.FilesRead++;
            for (int i = 1; i < lines.Length; i++) AddLine(lines[i], cutoff, result);
        }

        /// <summary>
        /// Reads all complete lines. The text after the last line break is dropped: the
        /// service may be halfway through writing it, and a cut number would look valid.
        /// </summary>
        /// <returns>The lines, or null when the file cannot be read (locked, no access, removed).</returns>
        private static string[]? TryReadLines(string path)
        {
            try
            {
                using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                using var reader = new StreamReader(stream);
                string[] parts = reader.ReadToEnd().Split('\n');
                return parts.Take(parts.Length - 1).ToArray();
            }
            catch (IOException) { return null; }
            catch (UnauthorizedAccessException) { return null; }
        }

        /// <summary>Parses one line and adds it, or counts it as bad, or drops it when older than the cutoff.</summary>
        private static void AddLine(string line, DateTime cutoff, HistoryReadResult result)
        {
            string text = line.TrimEnd('\r');
            if (string.IsNullOrWhiteSpace(text)) return;
            HistoryRow? row = ParseRow(text);
            if (row == null) { result.BadRows++; return; }
            if (row.EndUtc < cutoff) return;
            result.Add(row);
        }

        private static bool TryTime(string text, out DateTime value) =>
            DateTime.TryParseExact(text, "yyyy-MM-dd'T'HH:mm:ss'Z'", CultureInfo.InvariantCulture,
                DateTimeStyles.AdjustToUniversal | DateTimeStyles.AssumeUniversal, out value);

        private static bool TrySeconds(string text, out int value)
        {
            bool ok = int.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out value);
            return ok && value >= 1 && value <= 86400;
        }

        private static bool TryState(string text, out int? value)
        {
            value = null;
            if (text == "") return true;
            if (text != "0" && text != "1" && text != "2") return false;
            value = text[0] - '0';
            return true;
        }

        /// <summary>Empty gives null and true. Otherwise a finite number inside min..max, else false.</summary>
        private static bool TryOptional(string text, double min, double max, out double? value)
        {
            value = null;
            if (text == "") return true;
            bool ok = double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double number);
            if (!ok || !double.IsFinite(number) || number < min || number > max) return false;
            value = number;
            return true;
        }
    }
}
