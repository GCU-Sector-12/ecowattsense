// Harness.cs: counts the checks of the new check groups (stream, trust check, graphics, conditions) and prints one line for each.
// Who uses it: the *Checks.cs files of this folder; Program.Main adds Failed to its own count and prints the summary.
// A group that throws is counted as one failed check, so one broken group never hides the others.
namespace CalibratorTests;

/// <summary>Counts checks and prints a line for each one.</summary>
internal static class Harness
{
    /// <summary>Number of checks run.</summary>
    public static int Total { get; private set; }

    /// <summary>Number of failed checks.</summary>
    public static int Failed { get; private set; }

    /// <summary>Records one check.</summary>
    /// <param name="ok">The condition that must be true.</param>
    /// <param name="name">What is checked.</param>
    public static void Check(bool ok, string name)
    {
        Total++;
        Failed += ok ? 0 : 1;
        Console.WriteLine((ok ? "PASS  " : "FAIL  ") + name);
    }

    /// <summary>Runs one group of checks. An exception counts as one failed check.</summary>
    /// <param name="name">The name of the group.</param>
    /// <param name="group">The group.</param>
    public static void Group(string name, Action group)
    {
        try { group(); }
        catch (Exception ex) { Check(false, $"{name} threw {ex.GetType().Name}"); }
    }

    /// <summary>Prints a line of information that is not a check.</summary>
    /// <param name="text">The text.</param>
    public static void Info(string text) => Console.WriteLine("INFO  " + text);

    /// <summary>Tells whether a value is within a tolerance of the expected one.</summary>
    /// <param name="actual">The value, or null.</param>
    /// <param name="expected">The expected value.</param>
    /// <param name="tolerance">The allowed difference.</param>
    /// <returns>True when the value is known and close enough.</returns>
    public static bool Near(double? actual, double expected, double tolerance = 1e-9) => actual.HasValue && Math.Abs(actual.Value - expected) <= tolerance;
}
