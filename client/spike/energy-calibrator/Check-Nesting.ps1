# Check-Nesting.ps1: the "NEVER NESTED" check for the C# files of EnergyCalibrator.
# Who uses it: the author of the code, the main session, and anyone who changes the code.
# How to run: pwsh -NoProfile -File Check-Nesting.ps1 [-Path <folder>] [-SelfTest]
#   -Path      folder with the .cs files (default: the folder of this script; the folders obj, bin and out are skipped)
#   -SelfTest  runs the checker on small examples with a known answer, so you can see that it finds nesting
# What it flags: a control statement (if, for, foreach, while, switch, try, do) that sits inside the block of another control
# statement (if, else, for, foreach, while, switch, try, catch, finally, do, using, lock), inside the single-statement body of one,
# or inside a lambda block. "else if" is a chain, not nesting. The keyword after "do { }" is the tail of the do-while, not a new loop.
# How it works: comments, string literals (also interpolated ones with quotes inside their holes) and char literals are blanked out;
# then the braces are tracked with a stack, so every method starts with an empty stack and the depth is counted per method.
# Exit code: 0 when there are no findings, 1 when there are findings or a self-test fails.
param(
    [string] $Path = $PSScriptRoot,
    [switch] $SelfTest
)

# The checker is C# inside a here-string (it is compiled once per run). It uses guard clauses and small methods, no nested control statements.
$source = @'
using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;

public class NestingFinding
{
    public int Line;
    public string Message;
}

public static class NestingChecker
{
    public static List<NestingFinding> Check(string text) { return new NestingScan().Run(text); }
}

class NestingFrame
{
    public char Kind;
    public string Keyword;
    public int Line;
    public NestingFrame(char kind, string keyword, int line) { Kind = kind; Keyword = keyword; Line = line; }
}

class NestingPending
{
    public string Keyword;
    public int Line;
    public int Depth;
    public NestingPending(string keyword, int line) { Keyword = keyword; Line = line; Depth = 0; }
}

class CodeBlanker
{
    string s;
    StringBuilder o;
    int i;

    public string Run(string text) { s = text; o = new StringBuilder(text.Length); i = 0; Code(false, true); return o.ToString(); }

    char Peek(int k) { return i + k < s.Length ? s[i + k] : '\0'; }

    void Blank(int count)
    {
        for (int k = 0; k < count && i < s.Length; k++) { o.Append(s[i] == '\n' || s[i] == '\r' ? s[i] : ' '); i++; }
    }

    void Emit(bool keep) { o.Append(keep || s[i] == '\n' || s[i] == '\r' ? s[i] : ' '); i++; }

    void Code(bool inHole, bool keep)
    {
        int depth = 0;
        while (i < s.Length && !(inHole && depth == 0 && s[i] == '}')) { depth += Step(keep, depth); }
    }

    int Step(bool keep, int depth)
    {
        char c = s[i];
        if (c == '/' && Peek(1) == '/') { BlankUntilLineEnd(); return 0; }
        if (c == '/' && Peek(1) == '*') { BlankBlockComment(); return 0; }
        if (c == '"') { Str(1, false, false); return 0; }
        if (c == '$' && Peek(1) == '"') { Str(2, false, true); return 0; }
        if (c == '@' && Peek(1) == '"') { Str(2, true, false); return 0; }
        if (c == '$' && Peek(1) == '@' && Peek(2) == '"') { Str(3, true, true); return 0; }
        if (c == '@' && Peek(1) == '$' && Peek(2) == '"') { Str(3, true, true); return 0; }
        if (c == '\'') { BlankChar(); return 0; }
        Emit(keep);
        if (c == '{') { return 1; }
        return c == '}' && depth > 0 ? -1 : 0;
    }

    void BlankUntilLineEnd() { while (i < s.Length && s[i] != '\n') { Blank(1); } }

    void BlankBlockComment() { Blank(2); while (i < s.Length && !(s[i] == '*' && Peek(1) == '/')) { Blank(1); } Blank(2); }

    void BlankChar() { Blank(1); while (i < s.Length && s[i] != '\'') { Blank(s[i] == '\\' ? 2 : 1); } Blank(1); }

    bool StringEnds(bool verbatim) { return s[i] == '"' && !(verbatim && Peek(1) == '"'); }

    void Str(int prefix, bool verbatim, bool interp)
    {
        Blank(prefix);
        while (i < s.Length && !StringEnds(verbatim)) { StringPart(verbatim, interp); }
        Blank(1);
    }

    void StringPart(bool verbatim, bool interp)
    {
        char c = s[i];
        if (!verbatim && c == '\\') { Blank(2); return; }
        if (verbatim && c == '"') { Blank(2); return; }
        if (interp && c == '{' && Peek(1) == '{') { Blank(2); return; }
        if (interp && c == '}' && Peek(1) == '}') { Blank(2); return; }
        if (interp && c == '{') { Hole(); return; }
        Blank(1);
    }

    void Hole() { Blank(1); Code(true, false); Blank(1); }
}

class NestingScan
{
    static readonly HashSet<string> WithParen = new HashSet<string> { "if", "for", "foreach", "while", "switch", "lock", "using", "fixed" };
    static readonly HashSet<string> Bare = new HashSet<string> { "try", "finally", "else", "do", "catch" };
    static readonly HashSet<string> Flagged = new HashSet<string> { "if", "for", "foreach", "while", "switch", "try", "do" };

    List<NestingFinding> findings = new List<NestingFinding>();
    Stack<NestingFrame> stack = new Stack<NestingFrame>();
    NestingPending pending;
    bool expectDoWhile;
    bool inDoTail;
    string previous = "";
    int line = 1;
    int lastIndex = 0;
    string clean;
    MatchCollection tokens;

    public List<NestingFinding> Run(string text)
    {
        clean = new CodeBlanker().Run(text);
        tokens = Regex.Matches(clean, @"[A-Za-z_][A-Za-z0-9_]*|=>|[{}();]");
        for (int t = 0; t < tokens.Count; t++) { Token(t); }
        Finish();
        return findings;
    }

    void Add(string message) { NestingFinding f = new NestingFinding(); f.Line = line; f.Message = message; findings.Add(f); }

    static bool IsControlStart(string v, string next) { return Bare.Contains(v) || (WithParen.Contains(v) && next == "("); }

    int CountNewlines(int from, int to) { int n = 0; for (int k = from; k < to; k++) { n += clean[k] == '\n' ? 1 : 0; } return n; }

    void Token(int t)
    {
        Match m = tokens[t];
        line += CountNewlines(lastIndex, m.Index);
        lastIndex = m.Index;
        string v = m.Value;
        string next = t + 1 < tokens.Count ? tokens[t + 1].Value : "";
        bool wasExpecting = expectDoWhile;
        expectDoWhile = false;
        if (inDoTail) { DoTail(v); }
        else if (wasExpecting && v == "while") { inDoTail = true; }
        else if (pending != null) { InPending(v, next); }
        else { Plain(v, next); }
        previous = v;
    }

    void DoTail(string v) { if (v == ";") { inDoTail = false; } }

    void InPending(string v, string next)
    {
        if (v == "(") { pending.Depth++; return; }
        if (v == ")") { pending.Depth--; return; }
        if (v == "{") { OpenBrace(pending.Depth == 0); return; }
        if (v == "}") { CloseBrace(); return; }
        if (v == ";" && pending.Depth == 0) { pending = null; return; }
        if (pending.Depth == 0 && IsControlStart(v, next)) { BracelessBody(v); }
    }

    void Plain(string v, string next)
    {
        if (v == "{") { OpenBrace(false); return; }
        if (v == "}") { CloseBrace(); return; }
        if (!IsControlStart(v, next)) { return; }
        if (Flagged.Contains(v)) { CheckEnclosing(v); }
        pending = new NestingPending(v, line);
    }

    void OpenBrace(bool ownsBlock)
    {
        if (ownsBlock) { stack.Push(new NestingFrame('c', pending.Keyword, pending.Line)); pending = null; return; }
        stack.Push(new NestingFrame(previous == "=>" ? 'l' : 'o', "", line));
    }

    void CloseBrace()
    {
        if (stack.Count == 0) { Add("closing brace without an opening one (check the string literals)"); return; }
        NestingFrame f = stack.Pop();
        expectDoWhile = f.Kind == 'c' && f.Keyword == "do";
    }

    void BracelessBody(string v)
    {
        bool elseIf = pending.Keyword == "else" && v == "if";
        if (!elseIf && Flagged.Contains(v)) { Add("'" + v + "' inside the single-statement body of '" + pending.Keyword + "' (line " + pending.Line + ")"); }
        pending = new NestingPending(v, line);
    }

    void CheckEnclosing(string v)
    {
        NestingFrame outer = Array.Find(stack.ToArray(), f => f.Kind == 'c' || f.Kind == 'l');
        if (outer == null) { return; }
        if (outer.Kind == 'l') { Add("'" + v + "' inside a lambda block (logic in a lambda)"); return; }
        Add("'" + v + "' inside the '" + outer.Keyword + "' block opened at line " + outer.Line);
    }

    void Finish()
    {
        if (stack.Count > 0) { Add(stack.Count + " block(s) never closed (check the string literals)"); }
    }
}
'@

if (-not ('NestingChecker' -as [type])) { Add-Type -TypeDefinition $source -Language CSharp }

<# Runs one self-test case. Takes a name, C# code and the number of findings the checker must report; returns true when it matches. #>
function Test-Case([string] $Name, [string] $Code, [int] $Expected) {
    $count = @([NestingChecker]::Check($Code)).Count
    $state = if ($count -eq $Expected) { 'PASS' } else { 'FAIL' }
    Write-Host ('{0}: {1} (found {2}, expected {3})' -f $state, $Name, $count, $Expected)
    return ($count -eq $Expected)
}

<# Runs all self-test cases. Returns the number of failed cases. #>
function Invoke-SelfTest {
    $cases = @(
        @('if inside foreach',          'class A { void M() { foreach (var a in b) { if (a) { x(); } } } }', 1),
        @('else-if chain is flat',      'class A { void M() { if (a) { x(); } else if (b) { y(); } else { z(); } } }', 0),
        @('sequential statements',      'class A { void M() { if (a) { return; } if (b) { return; } try { x(); } catch (Exception e) { y(); } } }', 0),
        @('braceless if inside if',     'class A { void M() { if (a) if (b) x(); } }', 1),
        @('if inside lambda block',     'class A { void M() { var f = () => { if (a) { x(); } }; } }', 1),
        @('while inside try',           'class A { void M() { try { while (a) { x(); } } finally { y(); } } }', 1),
        @('do-while is one loop',       'class A { void M() { do { x(); } while (a); } }', 0),
        @('if in a property getter',    'class A { int P { get { if (a) { return 1; } return 2; } } }', 0),
        @('braces in a string',         'class A { void M() { var s = "{ if (a) { } "; } }', 0),
        @('quotes in interpolation',    'class A { void M() { var s = $"{Foo(a, "x")} { if }"; if (a) { x(); } } }', 0),
        @('if inside switch',           'class A { void M() { switch (a) { case 1: if (b) { x(); } break; } } }', 1),
        @('switch expression is flat',  'class A { int M() { if (a) { return 1; } return b switch { 1 => 2, _ => 3 }; } }', 0),
        @('for inside else block',      'class A { void M() { if (a) { x(); } else { for (;;) { y(); } } } }', 1)
    )
    $results = foreach ($case in $cases) { Test-Case $case[0] $case[1] $case[2] }
    return @($results | Where-Object { -not $_ }).Count
}

<# Checks one file. Takes the file; writes one text line per finding. #>
function Get-Findings($file) {
    [NestingChecker]::Check([IO.File]::ReadAllText($file.FullName)) | ForEach-Object { '{0}:{1}: {2}' -f $file.Name, $_.Line, $_.Message }
}

if ($SelfTest) {
    $failed = Invoke-SelfTest
    Write-Host ('{0} self-test(s) failed' -f $failed)
    exit ([int]($failed -gt 0))
}

$files = @(Get-ChildItem -Path $Path -Filter *.cs -Recurse | Where-Object { $_.FullName -notmatch '[\\/](obj|bin|out)[\\/]' })
$lines = @($files | ForEach-Object { Get-Findings $_ })
$lines | ForEach-Object { Write-Host $_ }
Write-Host ('{0} finding(s) in {1} file(s)' -f $lines.Count, $files.Count)
exit ([int]($lines.Count -gt 0))
