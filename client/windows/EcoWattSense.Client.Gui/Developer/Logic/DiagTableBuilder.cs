// DiagTableBuilder.cs: makes a table from rows whose fields the page does not know (spec D2, section 2.2).
// Who uses it: DiagSnapshotLoader and the tests.
// The columns are the union of all names, in the order of first appearance in the rows from oldest to newest, so a new
// field is added at the end and the columns do not jump while new lines arrive. The names the caller asks for ("ts" and
// so on) go first. A row that has no value for a column gets an empty cell. The rows are returned newest first.
// At most 5000 rows (the newest) and 400 columns.
#nullable enable
using System;
using System.Collections.Generic;
using System.Linq;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Builds a <see cref="DiagTable"/>.</summary>
public static class DiagTableBuilder
{
    private const int WidthRows = 100;
    private const int MinWidth = 6;
    private const int MaxWidth = 40;

    /// <summary>Builds the table.</summary>
    /// <param name="chronological">The rows, oldest first.</param>
    /// <param name="firstColumns">The columns that go first, in this order. A name that no row has is not added.</param>
    /// <returns>The table. No rows give an empty table.</returns>
    public static DiagTable Build(IReadOnlyList<DiagRow> chronological, params string[] firstColumns)
    {
        int skip = Math.Max(0, chronological.Count - DiagLimits.MaxRows);
        List<DiagRow> kept = chronological.Skip(skip).ToList();
        var set = new ColumnSet(firstColumns);
        foreach (DiagRow row in kept) set.AddFrom(row);
        List<string> columns = set.Ordered();
        Dictionary<string, int> index = Enumerable.Range(0, columns.Count).ToDictionary(i => columns[i]);
        var rows = new List<string[]>(kept.Count);
        foreach (DiagRow row in kept) rows.Add(ToCells(row, columns.Count, index));
        rows.Reverse();
        return new DiagTable(columns, rows, Widths(columns, rows));
    }

    /// <summary>Puts the values of a row into an array with one slot for each column.</summary>
    /// <param name="row">The row.</param>
    /// <param name="width">The number of columns.</param>
    /// <param name="index">The position of each column name.</param>
    /// <returns>The values. A column the row does not have holds an empty text.</returns>
    private static string[] ToCells(DiagRow row, int width, Dictionary<string, int> index)
    {
        var cells = new string[width];
        Array.Fill(cells, "");
        foreach (KeyValuePair<string, string> cell in row.Cells) SetCell(cells, index, cell);
        return cells;
    }

    /// <summary>Stores one value in its column. A name that is not a column (cut by the column limit) is dropped.</summary>
    /// <param name="cells">The row array.</param>
    /// <param name="index">The position of each column name.</param>
    /// <param name="cell">The name and value.</param>
    private static void SetCell(string[] cells, Dictionary<string, int> index, KeyValuePair<string, string> cell)
    {
        if (index.TryGetValue(cell.Key, out int position)) cells[position] = cell.Value;
    }

    /// <summary>Works out a column width in characters for every column.</summary>
    /// <param name="columns">The column names.</param>
    /// <param name="rows">The rows, newest first.</param>
    /// <returns>One width for each column, from 6 to 40.</returns>
    private static List<int> Widths(List<string> columns, List<string[]> rows)
        => Enumerable.Range(0, columns.Count).Select(i => WidthOf(columns[i], rows, i)).ToList();

    /// <summary>Works out the width of one column from its name and its first values.</summary>
    /// <param name="name">The column name.</param>
    /// <param name="rows">The rows, newest first.</param>
    /// <param name="column">The column position.</param>
    /// <returns>The width in characters, from 6 to 40.</returns>
    private static int WidthOf(string name, List<string[]> rows, int column)
    {
        int longest = rows.Take(WidthRows).Select(r => r[column].Length).DefaultIfEmpty(0).Max();
        return Math.Clamp(Math.Max(name.Length, longest) + 1, MinWidth, MaxWidth);
    }

    /// <summary>Collects the column names of many rows.</summary>
    private sealed class ColumnSet
    {
        private readonly string[] first;
        private readonly List<string> names = new();
        private readonly HashSet<string> seen = new();

        /// <summary>Creates the set.</summary>
        /// <param name="first">The columns that go first.</param>
        public ColumnSet(string[] first) => this.first = first;

        /// <summary>Adds the names of a row that are new.</summary>
        /// <param name="row">The row.</param>
        public void AddFrom(DiagRow row)
        {
            foreach (KeyValuePair<string, string> cell in row.Cells) Add(cell.Key);
        }

        /// <summary>Adds one name when it is new and the column limit is not reached.</summary>
        /// <param name="name">The column name.</param>
        private void Add(string name)
        {
            if (names.Count >= DiagLimits.MaxColumns) return;
            if (seen.Add(name)) names.Add(name);
        }

        /// <summary>Gives the names with the wanted first columns at the front.</summary>
        /// <returns>The names. The first columns that exist, then the others in the order of first appearance.</returns>
        public List<string> Ordered()
        {
            List<string> front = first.Where(seen.Contains).Distinct().ToList();
            return front.Concat(names.Where(n => !front.Contains(n))).ToList();
        }
    }
}
