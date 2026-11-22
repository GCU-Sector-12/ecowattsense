// TextCleaner.cs: makes a text from a file safe and short enough for a table cell or a log line.
// Who uses it: JsonFlattener (names and values), LogTailReader (log lines) and the tests.
// Control characters, line breaks and the invisible direction marks are turned into spaces, so a cell is always one
// plain line and a hostile file cannot reorder what the user reads. A text over the limit ends with "...".
#nullable enable
using System.Globalization;
using System.Text;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Cleans and cuts text that came from a file.</summary>
public static class TextCleaner
{
    private const string Dots = "...";

    /// <summary>Replaces unsafe characters with spaces and cuts the text.</summary>
    /// <param name="text">The text from the file.</param>
    /// <param name="maxChars">The most characters in the result, including the dots.</param>
    /// <returns>The cleaned text. A text of exactly maxChars characters is not cut.</returns>
    public static string Clean(string text, int maxChars)
    {
        string head = text.Length > maxChars + 1 ? text.Substring(0, maxChars + 1) : text;
        return Cut(ReplaceUnsafe(head), maxChars);
    }

    /// <summary>Replaces each unsafe character with a space.</summary>
    /// <param name="text">The text to clean.</param>
    /// <returns>The text with only safe characters.</returns>
    private static string ReplaceUnsafe(string text)
    {
        var builder = new StringBuilder(text.Length);
        foreach (char c in text) builder.Append(IsUnsafe(c) ? ' ' : c);
        return builder.ToString();
    }

    /// <summary>Tells whether a character must not reach the screen.</summary>
    /// <param name="c">The character.</param>
    /// <returns>True for control characters, format characters (direction marks) and line or paragraph separators.</returns>
    private static bool IsUnsafe(char c)
    {
        if (char.IsControl(c)) return true;
        UnicodeCategory category = char.GetUnicodeCategory(c);
        return category is UnicodeCategory.Format or UnicodeCategory.LineSeparator or UnicodeCategory.ParagraphSeparator;
    }

    /// <summary>Cuts a text that is too long and adds the dots. Never leaves half of a surrogate pair.</summary>
    /// <param name="text">The cleaned text.</param>
    /// <param name="maxChars">The most characters in the result.</param>
    /// <returns>The text, or its start followed by "...".</returns>
    private static string Cut(string text, int maxChars)
    {
        if (text.Length <= maxChars) return text;
        int keep = Math.Max(0, maxChars - Dots.Length);
        bool splitsPair = keep > 0 && char.IsHighSurrogate(text[keep - 1]);
        return text.Substring(0, splitsPair ? keep - 1 : keep) + Dots;
    }
}
