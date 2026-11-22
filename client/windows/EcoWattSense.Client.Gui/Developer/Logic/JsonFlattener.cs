// JsonFlattener.cs: turns one JSON value into a flat list of "name, value" pairs (spec D2, section 2.2).
// Who uses it: DiagLineParser (every line of the diag files), JsonFileRows (Sources and Status tabs) and the tests.
// The page does not know the field names of the service. It walks whatever it reads: an object adds ".name", an array of
// objects adds "[0]", an array of plain values becomes one cell joined with a space ("flags" gives "gap held").
// Names and values are cleaned and cut to 200 characters. The number of cells is capped, so one line cannot make a huge row.
#nullable enable
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Flattens JSON into name and value pairs.</summary>
public static class JsonFlattener
{
    /// <summary>Flattens a value with the column limit as the most cells.</summary>
    /// <param name="root">The JSON value, normally an object.</param>
    /// <returns>The pairs in the order of the JSON. At most 400.</returns>
    public static List<KeyValuePair<string, string>> Flatten(JsonElement root) => Flatten(root, DiagLimits.MaxColumns);

    /// <summary>Flattens a value.</summary>
    /// <param name="root">The JSON value, normally an object.</param>
    /// <param name="maxCells">The most pairs to return. The rest is left out.</param>
    /// <returns>The pairs in the order of the JSON.</returns>
    public static List<KeyValuePair<string, string>> Flatten(JsonElement root, int maxCells)
    {
        var walker = new Walker(maxCells);
        walker.Add("", root);
        return walker.Cells;
    }

    /// <summary>The walk over one JSON value. It collects the cells.</summary>
    private sealed class Walker
    {
        private readonly int maxCells;

        /// <summary>Creates a walker.</summary>
        /// <param name="maxCells">The most cells to collect.</param>
        public Walker(int maxCells) => this.maxCells = maxCells;

        /// <summary>The cells collected so far.</summary>
        public List<KeyValuePair<string, string>> Cells { get; } = new();

        /// <summary>Adds a value under a name. An object or an array adds its parts.</summary>
        /// <param name="name">The flattened name so far ("" at the top).</param>
        /// <param name="value">The value.</param>
        public void Add(string name, JsonElement value)
        {
            if (Cells.Count >= maxCells) return;
            switch (value.ValueKind)
            {
                case JsonValueKind.Object: AddObject(name, value); break;
                case JsonValueKind.Array: AddArray(name, value); break;
                default: AddCell(name, ScalarText(value)); break;
            }
        }

        /// <summary>Adds every property of an object.</summary>
        /// <param name="name">The name of the object.</param>
        /// <param name="value">The object.</param>
        private void AddObject(string name, JsonElement value)
        {
            foreach (JsonProperty property in value.EnumerateObject()) AddProperty(name, property);
        }

        /// <summary>Adds one property under "name.property".</summary>
        /// <param name="name">The name of the object that holds it.</param>
        /// <param name="property">The property.</param>
        private void AddProperty(string name, JsonProperty property)
            => Add(name.Length == 0 ? property.Name : name + "." + property.Name, property.Value);

        /// <summary>Adds an array: one joined cell for plain values, else one part for each element.</summary>
        /// <param name="name">The name of the array.</param>
        /// <param name="value">The array.</param>
        private void AddArray(string name, JsonElement value)
        {
            if (value.EnumerateArray().All(IsScalar))
            {
                AddCell(name, JoinScalars(value));
                return;
            }
            AddElements(name, value);
        }

        /// <summary>Adds the elements of an array as "name[0]", "name[1]" and so on.</summary>
        /// <param name="name">The name of the array.</param>
        /// <param name="value">The array.</param>
        private void AddElements(string name, JsonElement value)
        {
            int index = 0;
            foreach (JsonElement element in value.EnumerateArray()) Add(name + "[" + index++ + "]", element);
        }

        /// <summary>Adds one cell with a cleaned name and value.</summary>
        /// <param name="name">The flattened name.</param>
        /// <param name="text">The value as text.</param>
        private void AddCell(string name, string text)
            => Cells.Add(new(TextCleaner.Clean(name, DiagLimits.MaxCellChars), TextCleaner.Clean(text, DiagLimits.MaxCellChars)));

        /// <summary>Tells whether a value is a plain value (not an object and not an array).</summary>
        /// <param name="value">The value.</param>
        /// <returns>True for a string, number, true, false or null.</returns>
        private static bool IsScalar(JsonElement value) => value.ValueKind is not (JsonValueKind.Object or JsonValueKind.Array);

        /// <summary>Joins the plain values of an array with a space. It stops when the text is already long enough for a cell.</summary>
        /// <param name="value">The array.</param>
        /// <returns>The joined text. Empty for an empty array.</returns>
        private static string JoinScalars(JsonElement value)
        {
            var builder = new StringBuilder();
            foreach (JsonElement element in value.EnumerateArray()) AppendScalar(builder, element);
            return builder.ToString();
        }

        /// <summary>Appends one plain value, after a space when the text is not empty.</summary>
        /// <param name="builder">The text so far.</param>
        /// <param name="element">The plain value.</param>
        private static void AppendScalar(StringBuilder builder, JsonElement element)
        {
            if (builder.Length > DiagLimits.MaxCellChars) return;
            if (builder.Length > 0) builder.Append(' ');
            builder.Append(ScalarText(element));
        }

        /// <summary>Gives the text of a plain value. A number keeps exactly the digits of the file.</summary>
        /// <param name="value">The value.</param>
        /// <returns>The text; "null" for a JSON null.</returns>
        private static string ScalarText(JsonElement value) => value.ValueKind switch
        {
            JsonValueKind.String => value.GetString() ?? "",
            JsonValueKind.Number => value.GetRawText(),
            JsonValueKind.True => "true",
            JsonValueKind.False => "false",
            JsonValueKind.Null => "null",
            _ => ""
        };
    }
}
