# Check-Nesting.ps1: the "NEVER NESTED" check for the C# and C++ files of the energy tools.
# Who uses it: the author of the code, the main session, and anyone who changes the code.
# How to run: pwsh -NoProfile -File Check-Nesting.ps1 [-Path <folder>] [-File <file>[,<file>...]] [-SelfTest]
#   -Path      folder to scan (default: the folder of this script). It takes .cs files (C#) and .cpp, .h, .hpp files (C++).
#              The folders obj, bin and out are skipped; for C++ also x64, Debug, Release and .vs.
#   -File      check exactly these files instead of a folder; the language comes from the extension (.cs, .cpp, .h, .hpp).
#              A file that does not exist, cannot be read or has another extension is reported as an error and gives exit code 1.
#   -SelfTest  runs the checker on small examples with a known answer, so you can see that it finds nesting
# What it flags: a control statement (C#: if, for, foreach, while, switch, try, do; C++: if, for, while, switch, try, do) that sits
# inside the block of another control statement (C# also: foreach, finally, using, lock; C++ also: else, catch), inside the
# single-statement body of one, or inside a lambda block. "else if" is a chain, not nesting. The keyword after "do { }" is the
# tail of the do-while, not a new loop. In C++ "if constexpr" counts as "if" and a range-based for counts as "for".
# How it works: comments, string literals and char literals are blanked out (C# also interpolated strings; C++ also raw strings
# R"x( ... )x", preprocessor lines with backslash continuation, and digit separators such as 1'000 are kept as code);
# then the braces are tracked with a stack, so every method starts with an empty stack and the depth is counted per method.
# A C++ lambda is "[" capture "]", optional "(" params ")", optional specifiers, then "{". Array indexes and [[attributes]] are not lambdas.
# Exit code: 0 when there are no findings, 1 when there are findings, an error line or a failed self-test.
param(
    [string] $Path = $PSScriptRoot,
    [string[]] $File,
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
    public static List<NestingFinding> CheckCpp(string text) { return new CppScan().Run(text); }
    public static List<NestingFinding> CheckFile(string text, string ext) { return ext == ".cs" ? Check(text) : CheckCpp(text); }
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

// C++ part. Same idea as the C# scan: blank what is not code, then follow the braces with a stack.
class CppBlanker
{
    string s;
    StringBuilder o;
    int i;

    public string Run(string text) { s = text; o = new StringBuilder(text.Length); i = 0; while (i < s.Length) { Step(); } return o.ToString(); }

    char Peek(int k) { return i + k < s.Length ? s[i + k] : '\0'; }

    void Blank(int count)
    {
        for (int k = 0; k < count && i < s.Length; k++) { o.Append(s[i] == '\n' || s[i] == '\r' ? s[i] : ' '); i++; }
    }

    void Emit() { o.Append(s[i]); i++; }

    void Step()
    {
        char c = s[i];
        if (c == '/' && Peek(1) == '/') { BlankUntilLineEnd(); return; }
        if (c == '/' && Peek(1) == '*') { BlankBlockComment(); return; }
        if (c == '#' && AtLineStart()) { BlankDirective(); return; }
        if (c == '"' && i > 0 && s[i - 1] == 'R') { BlankRaw(); return; }
        if (c == '"') { BlankQuoted('"'); return; }
        if (c == '\'' && IsDigitSeparator()) { Emit(); return; }
        if (c == '\'') { BlankQuoted('\''); return; }
        Emit();
    }

    void BlankUntilLineEnd() { while (i < s.Length && s[i] != '\n') { Blank(1); } }

    void BlankBlockComment() { Blank(2); while (i < s.Length && !(s[i] == '*' && Peek(1) == '/')) { Blank(1); } Blank(2); }

    // A "#" that is the first thing on its line starts a preprocessor line.
    bool AtLineStart()
    {
        int k = i - 1;
        while (k >= 0 && (s[k] == ' ' || s[k] == '\t')) { k--; }
        return k < 0 || s[k] == '\n' || s[k] == '\r';
    }

    // Length of what must be blanked at once: a backslash with its line break, or one character.
    int DirectiveStep()
    {
        if (s[i] != '\\') { return 1; }
        if (Peek(1) == '\n') { return 2; }
        return Peek(1) == '\r' && Peek(2) == '\n' ? 3 : 1;
    }

    void BlankDirective() { while (i < s.Length && s[i] != '\n') { Blank(DirectiveStep()); } }

    // A string or char literal ends at its closing quote; an unterminated one ends at the line end, so one bad quote cannot hide the whole file.
    void BlankQuoted(char quote)
    {
        Blank(1);
        while (i < s.Length && s[i] != quote && s[i] != '\n') { Blank(s[i] == '\\' ? 2 : 1); }
        Blank(i < s.Length && s[i] == quote ? 1 : 0);
    }

    // The quote of R"delim( ... )delim". An unterminated raw string is blanked to the end of the file.
    void BlankRaw()
    {
        int open = s.IndexOf('(', i);
        if (open < 0 || open - i > 17) { BlankQuoted('"'); return; }
        string close = ")" + s.Substring(i + 1, open - i - 1) + "\"";
        int end = s.IndexOf(close, open + 1, StringComparison.Ordinal);
        Blank((end < 0 ? s.Length : end + close.Length) - i);
    }

    // 1'000 is a number: the quote follows a word that starts with a digit and is followed by a letter or digit.
    bool IsDigitSeparator()
    {
        int k = i - 1;
        while (k >= 0 && (char.IsLetterOrDigit(s[k]) || s[k] == '_' || s[k] == '\'')) { k--; }
        return k < i - 1 && char.IsDigit(s[k + 1]) && char.IsLetterOrDigit(Peek(1));
    }
}

// Finds the "{" that opens a lambda body. Works on blanked text and returns the index of each such brace.
static class CppLambdas
{
    static readonly HashSet<string> NotIndexWords = new HashSet<string> { "return", "co_return", "co_yield", "co_await", "throw", "case", "else", "do", "and", "or", "not" };

    public static HashSet<int> Find(string s)
    {
        HashSet<int> braces = new HashSet<int>();
        for (int k = 0; k < s.Length; k++) { AddBody(s, k, braces); }
        return braces;
    }

    static void AddBody(string s, int k, HashSet<int> braces)
    {
        if (s[k] != '[' || !IsIntroducer(s, k)) { return; }
        int close = MatchBracket(s, k);
        if (close < 0) { return; }
        int brace = BodyBrace(s, close + 1);
        if (brace >= 0) { braces.Add(brace); }
    }

    static bool IsWord(char c) { return char.IsLetterOrDigit(c) || c == '_'; }

    static int Delta(char c) { return c == '[' ? 1 : c == ']' ? -1 : 0; }

    // Index of the "]" that closes the "[" at open, or -1.
    static int MatchBracket(string s, int open)
    {
        int depth = 1;
        int k = open + 1;
        while (k < s.Length && depth > 0) { depth += Delta(s[k]); k++; }
        return depth == 0 ? k - 1 : -1;
    }

    static string WordEndingAt(string s, int j)
    {
        int e = j;
        while (e >= 0 && IsWord(s[e])) { e--; }
        return s.Substring(e + 1, j - e);
    }

    // "[[" starts an attribute; after a name, ")" or "]" the bracket is an index; anywhere else it can start a lambda.
    static bool IsIntroducer(string s, int k)
    {
        if (k + 1 < s.Length && s[k + 1] == '[') { return false; }
        int j = k - 1;
        while (j >= 0 && char.IsWhiteSpace(s[j])) { j--; }
        if (j < 0) { return true; }
        if (s[j] == ')' || s[j] == ']') { return false; }
        if (!IsWord(s[j])) { return true; }
        return NotIndexWords.Contains(WordEndingAt(s, j));
    }

    // After the capture: parameters, specifiers and a trailing return type may follow, but no "=" or ";". Gives the "{" or -1.
    static int BodyBrace(string s, int from)
    {
        int k = from;
        while (k < s.Length && (IsWord(s[k]) || char.IsWhiteSpace(s[k]) || ":<>-,*&()".IndexOf(s[k]) >= 0)) { k++; }
        return k < s.Length && s[k] == '{' ? k : -1;
    }
}

class CppFrame
{
    public char Kind;
    public string Keyword;
    public int Line;
    public CppPending Saved;
    public CppFrame(char kind, string keyword, int line, CppPending saved) { Kind = kind; Keyword = keyword; Line = line; Saved = saved; }
}

class CppPending
{
    public string Keyword;
    public int Line;
    public int Depth;
    public bool HeaderDone;
    public bool BodyStarted;
    public CppPending(string keyword, int line, bool headerDone) { Keyword = keyword; Line = line; Depth = 0; HeaderDone = headerDone; BodyStarted = false; }
}

class CppScan
{
    static readonly HashSet<string> WithParen = new HashSet<string> { "if", "for", "while", "switch", "catch" };
    static readonly HashSet<string> Bare = new HashSet<string> { "try", "else", "do" };
    static readonly HashSet<string> Flagged = new HashSet<string> { "if", "for", "while", "switch", "try", "do" };

    List<NestingFinding> findings = new List<NestingFinding>();
    Stack<CppFrame> stack = new Stack<CppFrame>();
    CppPending pending;
    HashSet<int> lambdas;
    bool expectDoWhile;
    bool inDoTail;
    int line = 1;
    int lastIndex = 0;
    string clean;
    MatchCollection tokens;

    public List<NestingFinding> Run(string text)
    {
        clean = new CppBlanker().Run(text);
        lambdas = CppLambdas.Find(clean);
        tokens = Regex.Matches(clean, @"[A-Za-z_][A-Za-z0-9_]*|[{}();]");
        for (int t = 0; t < tokens.Count; t++) { Token(t); }
        Finish();
        return findings;
    }

    void Add(string message) { NestingFinding f = new NestingFinding(); f.Line = line; f.Message = message; findings.Add(f); }

    // "if constexpr (" and "if consteval {" are an if; the other keywords need "(" right after them.
    static bool IsControlStart(string v, string next)
    {
        if (Bare.Contains(v)) { return true; }
        if (v == "if" && (next == "constexpr" || next == "consteval")) { return true; }
        return WithParen.Contains(v) && next == "(";
    }

    static bool IsControlFrame(CppFrame f) { return f.Kind == 'c' || f.Kind == 'l'; }

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
        else if (pending != null) { InPending(v, next, m.Index); }
        else { Plain(v, next, m.Index); }
    }

    void DoTail(string v) { if (v == ";") { inDoTail = false; } }

    void InPending(string v, string next, int index)
    {
        if (v == "(") { OpenParen(); return; }
        if (v == ")") { CloseParen(); return; }
        if (v == "{") { OpenBrace(OwnsBlock(), index); return; }
        if (v == "}") { CloseBrace(); return; }
        if (v == ";" && pending.Depth == 0) { pending = null; return; }
        if (pending.Depth > 0) { return; }
        HeaderOrBody(v, next);
    }

    // A "(" right after the header starts the single-statement body.
    void OpenParen() { pending.BodyStarted = pending.BodyStarted || (pending.HeaderDone && pending.Depth == 0); pending.Depth++; }

    void CloseParen() { pending.Depth--; pending.HeaderDone = pending.HeaderDone || pending.Depth == 0; }

    // The "{" belongs to the keyword only when the header is complete and no body statement has started ("if (a) x = T{1};" is not a block).
    bool OwnsBlock() { return pending.HeaderDone && pending.Depth == 0 && !pending.BodyStarted; }

    void HeaderOrBody(string v, string next)
    {
        if (!pending.HeaderDone) { pending.HeaderDone = v == "consteval"; return; }
        if (IsControlStart(v, next)) { BracelessBody(v); return; }
        pending.BodyStarted = true;
    }

    void Plain(string v, string next, int index)
    {
        if (v == "{") { OpenBrace(false, index); return; }
        if (v == "}") { CloseBrace(); return; }
        if (!IsControlStart(v, next)) { return; }
        if (Flagged.Contains(v)) { CheckEnclosing(v); }
        pending = new CppPending(v, line, Bare.Contains(v));
    }

    // A brace that is not the block of the pending keyword keeps that keyword aside, so code inside it (a lambda in a condition) is scanned fresh.
    void OpenBrace(bool ownsBlock, int index)
    {
        if (ownsBlock) { stack.Push(new CppFrame('c', pending.Keyword, pending.Line, null)); pending = null; return; }
        stack.Push(new CppFrame(lambdas.Contains(index) ? 'l' : 'o', "", line, pending));
        pending = null;
    }

    void CloseBrace()
    {
        if (stack.Count == 0) { Add("closing brace without an opening one (check the string literals)"); return; }
        CppFrame f = stack.Pop();
        expectDoWhile = f.Kind == 'c' && f.Keyword == "do";
        pending = f.Saved;
    }

    void BracelessBody(string v)
    {
        bool elseIf = pending.Keyword == "else" && v == "if";
        if (!elseIf && Flagged.Contains(v)) { Add("'" + v + "' inside the single-statement body of '" + pending.Keyword + "' (line " + pending.Line + ")"); }
        pending = new CppPending(v, line, Bare.Contains(v));
    }

    void CheckEnclosing(string v)
    {
        CppFrame outer = Array.Find(stack.ToArray(), IsControlFrame);
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

# File extensions the checker understands, and the folders it skips (matched on the path below -Path).
$csExtensions = @('.cs')
$cppExtensions = @('.cpp', '.h', '.hpp')
$csSkip = '[\\/](obj|bin|out)[\\/]'
$cppSkip = '[\\/](obj|bin|out|x64|Debug|Release|\.vs)[\\/]'

<# Runs one self-test case. Takes a name, code, the number of findings the checker must report and the file extension (picks C# or C++); returns true when it matches. #>
function Test-Case([string] $Name, [string] $Code, [int] $Expected, [string] $Ext) {
    $count = @([NestingChecker]::CheckFile($Code, $Ext)).Count
    $state = if ($count -eq $Expected) { 'PASS' } else { 'FAIL' }
    Write-Host ('{0}: {1} (found {2}, expected {3})' -f $state, ($Ext + ' ' + $Name), $count, $Expected)
    return ($count -eq $Expected)
}

<# Runs a list of cases (name, code, expected count) for one extension. Returns one true or false per case. #>
function Invoke-Cases($cases, [string] $Ext) {
    foreach ($case in $cases) { Test-Case $case[0] $case[1] $case[2] $Ext }
}

<# The C# self-test cases: name, code, number of findings the checker must report. #>
function Get-CsCases {
    $csCases = @(
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
    return $csCases
}

<# The C++ self-test cases: name, code, number of findings the checker must report. Single quotes in C++ code are doubled for PowerShell. #>
function Get-CppCases {
    $nl = "`n"
    $cppCases = @(
        @('flat function',              'void f(int a) { if (a) { x(); } for (;;) { y(); } while (a) { z(); } }', 0),
        @('if inside for',              'void f() { for (int i = 0; i < 3; i++) { if (i) { x(); } } }', 1),
        @('else-if chain is flat',      'void f() { if (a) { x(); } else if (b) { y(); } else { z(); } }', 0),
        @('do-while is one loop',       'void f() { do { x(); } while (a); }', 0),
        @('braceless for with if',      'void f() { for (int i = 0; i < 3; i++) if (i) x(); }', 1),
        @('try inside while',           'void f() { while (a) { try { x(); } catch (...) { y(); } } }', 1),
        @('if inside catch',            'void f() { try { x(); } catch (const E& e) { if (a) { y(); } } }', 1),
        @('lambda with if',             'void f() { auto g = [&](int a) { if (a) { return 1; } return 0; }; }', 1),
        @('lambda without control',     'void f() { auto g = [](int a) { return a + 1; }; int v[3]; v[0] = 1; [[maybe_unused]] int u = 0; }', 0),
        @('string, raw string, comment', ('void f() { auto s = "{ if (a) { "; auto r = R"x( ) { if (b) { for (;;) { } )x"; /* { for (;;) { if (c) { */ // { if (d) {' + $nl + '}'), 0),
        @('define with backslash',      "#define M(a) if (a) { \`n for (;;) { } \`n}`nvoid f() { M(1); }", 0),
        @('if constexpr inside if',     'void f() { if (a) { if constexpr (b) { x(); } } }', 1),
        @('digit separator then if',    'void f() { int n = 1''000; for (;;) { if (n) { x(); } } }', 1),
        @('if inside switch case',      'void f() { switch (a) { case 1: if (b) { x(); } break; } }', 1),
        @('two flat methods',           'class A { void f() { if (a) { x(); } } void g() { if (b) { y(); } } };', 0),
        @('char literals are blanked',  'void f() { char c = ''{''; char d = ''\'''';  if (c) { x(); } }', 0),
        @('range-based for with if',    'void f() { for (auto& [k, v] : m) { if (v) { x(); } } }', 1)
    )
    return $cppCases
}

<# Runs all self-test cases. Returns the number of failed cases. #>
function Invoke-SelfTest {
    $results = @(Invoke-Cases (Get-CsCases) '.cs') + @(Invoke-Cases (Get-CppCases) '.cpp')
    return @($results | Where-Object { -not $_ }).Count
}

<# Tells whether a found file is one the checker wants. Takes the file and the scan root; returns true for a .cs or C++ file outside the skipped folders. #>
function Test-Wanted($item, [string] $RootFull) {
    $ext = $item.Extension.ToLowerInvariant()
    $below = '\' + $item.FullName.Substring($RootFull.Length).TrimStart('\', '/')
    if ($csExtensions -contains $ext) { return $below -notmatch $csSkip }
    if ($cppExtensions -contains $ext) { return $below -notmatch $cppSkip }
    return $false
}

<# Lists the files below a folder that the checker wants. Takes the folder; returns full paths. #>
function Get-FolderTargets([string] $Root) {
    $rootFull = (Resolve-Path -LiteralPath $Root).Path
    $items = Get-ChildItem -LiteralPath $rootFull -Recurse -File | Where-Object { Test-Wanted $_ $rootFull }
    return @($items | ForEach-Object { $_.FullName })
}

<# Checks one file. Takes its path; returns one text line per finding. A missing file, an unreadable file or an unknown extension gives one error line. #>
function Get-Findings([string] $FilePath) {
    $name = [IO.Path]::GetFileName($FilePath)
    $ext = [IO.Path]::GetExtension($FilePath).ToLowerInvariant()
    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) { return @("${name}:0: error: file not found: $FilePath") }
    if (-not (($csExtensions + $cppExtensions) -contains $ext)) { return @("${name}:0: error: unsupported file type '$ext'") }
    try { $text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $FilePath).Path) } catch { return @("${name}:0: error: cannot read the file") }
    return @([NestingChecker]::CheckFile($text, $ext) | ForEach-Object { '{0}:{1}: {2}' -f $name, $_.Line, $_.Message })
}

<# Gives the files to check: the -File list, or every wanted file below -Path. #>
function Get-Targets {
    if ($File) { return @($File) }
    return @(Get-FolderTargets $Path)
}

if (-not (Test-Path -LiteralPath $Path -PathType Container) -and -not $File -and -not $SelfTest) {
    Write-Host ('error: folder not found: {0}' -f $Path)
    exit 1
}

if ($SelfTest) {
    $failed = Invoke-SelfTest
    Write-Host ('{0} self-test(s) failed' -f $failed)
    exit ([int]($failed -gt 0))
}

$targets = @(Get-Targets)
$lines = @($targets | ForEach-Object { Get-Findings $_ })
$lines | ForEach-Object { Write-Host $_ }
Write-Host ('{0} finding(s) in {1} file(s)' -f $lines.Count, $targets.Count)
exit ([int]($lines.Count -gt 0))
