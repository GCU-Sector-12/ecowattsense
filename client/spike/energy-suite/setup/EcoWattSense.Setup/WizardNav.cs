// WizardNav.cs: the page rules of the install wizard (Scan, Server, Install, Calibration).
// Who uses it: SetupWindow asks it whether Back and a sidebar click are allowed and which page to show; the tests
// check it without a window. Rule: before the install has run, Back and a click on a step already reached are allowed;
// after the install has run, earlier steps are locked, because changing them would need a new install.
namespace EcoWattSense.Setup;

/// <summary>Tracks the current page, the furthest page reached and whether the install has run.</summary>
public sealed class WizardNav
{
    /// <summary>The page shown now.</summary>
    public int Page { get; private set; }

    /// <summary>The furthest page reached so far.</summary>
    public int Reached { get; private set; }

    /// <summary>True after the install has run: earlier steps are locked.</summary>
    public bool Locked { get; private set; }

    /// <summary>True when Back may go to the previous page.</summary>
    public bool CanGoBack => !Locked && Page > 0;

    /// <summary>True when a click on this step may show it: a step already reached, not the current one, not locked.</summary>
    /// <param name="page">The page index of the step.</param>
    public bool CanGoTo(int page) => !Locked && page >= 0 && page <= Reached && page != Page;

    /// <summary>Records that a page is shown. Going forward raises the furthest page reached.</summary>
    /// <param name="page">The page index.</param>
    public void Show(int page)
    {
        Page = page;
        Reached = Math.Max(Reached, page);
    }

    /// <summary>Goes one page back when allowed.</summary>
    /// <returns>The page now shown (unchanged when Back is not allowed).</returns>
    public int Back()
    {
        if (CanGoBack) Page--;
        return Page;
    }

    /// <summary>Locks the earlier steps once the install has run.</summary>
    public void Lock() => Locked = true;
}
