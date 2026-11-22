// Program.cs of DeveloperPage: plain console tests of the Developer page logic (spec D2, section 2.4).
// Who uses it: the author and the reviewer, with `dotnet run --project client/windows/tests/DeveloperPage/DeveloperPage.csproj`.
// It needs no Windows and no WPF. The logic files are compiled from EcoWattSense.Client.Gui/Developer/Logic.
// Every check prints one line. The exit code is the number of failed checks. Temporary folders are deleted at the end.
// A flat style is used on purpose: one Check call per fact, helper methods for set-up, no control statement inside another.
#nullable enable
using System.IO.Compression;
using System.Text;
using System.Text.Json;
using EcoWattSense.Client.Gui.Developer;

internal static class Program
{
    private static int checks;
    private static int failed;
    private static readonly List<string> TempDirs = new();
    private static readonly DateTimeOffset Now = new(2026, 11, 22, 12, 0, 0, TimeSpan.Zero);

    private static int Main()
    {
        TextCleanerTests();
        FlattenerTests();
        LineParserTests();
        TableBuilderTests();
        FileReaderTests();
        JsonFileRowsTests();
        LogTailTests();
        DeveloperModeTests();
        StatusMaskerTests();
        ExportPlanTests();
        ZipWriterTests();
        TextTests();
        SnapshotTests();
        CleanUp();
        Console.WriteLine($"{checks - failed} of {checks} checks passed, {failed} failed");
        return failed;
    }

    private static void Check(bool ok, string name)
    {
        checks++;
        failed += ok ? 0 : 1;
        Console.WriteLine((ok ? "PASS " : "FAIL ") + name);
    }

    /// <summary>Runs one check body and turns an exception into a failed check, so the red run lists every check.</summary>
    private static void Run(string name, Func<bool> body)
    {
        try { Check(body(), name); }
        catch (Exception exception) { Check(false, name + " (" + exception.GetType().Name + ")"); }
    }

    private static string NewDir()
    {
        string dir = Path.Combine(Path.GetTempPath(), "EWS-DevPage-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        TempDirs.Add(dir);
        return dir;
    }

    private static void CleanUp()
    {
        foreach (string dir in TempDirs) Directory.Delete(dir, true);
    }

    private static void Put(string dir, string relative, string text)
    {
        string path = Path.Combine(dir, relative);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, text, new UTF8Encoding(false));
    }

    private static string Tick(string ts, string extra = "") => "{\"t\":\"tick\",\"ts\":\"" + ts + "\",\"seconds\":60" + extra + "}";

    private static JsonElement Json(string text) => JsonDocument.Parse(text).RootElement.Clone();

    private static string Value(IReadOnlyList<KeyValuePair<string, string>> cells, string name)
        => cells.FirstOrDefault(c => c.Key == name).Value ?? "<missing>";

    // ---------------------------------------------------------------- TextCleaner
    private static void TextCleanerTests()
    {
        Run("cleaner: short text stays", () => TextCleaner.Clean("abc", 200) == "abc");
        Run("cleaner: control chars become spaces", () => TextCleaner.Clean("a\nb\tc\u0000d", 200) == "a b c d");
        Run("cleaner: bidi override removed", () => TextCleaner.Clean("a‮b", 200) == "a b");
        Run("cleaner: 200 chars stay", () => TextCleaner.Clean(new string('x', 200), 200).Length == 200);
        Run("cleaner: 201 chars are cut to 200 with dots", () => TextCleaner.Clean(new string('x', 201), 200) == new string('x', 197) + "...");
        Run("cleaner: no half surrogate pair", () => !char.IsHighSurrogate(TextCleaner.Clean(new string('x', 196) + "😀" + "yyyy", 200).TrimEnd('.').Last()));
    }

    // ---------------------------------------------------------------- JsonFlattener
    private static void FlattenerTests()
    {
        var cells = JsonFlattener.Flatten(Json("{\"t\":\"tick\",\"calc\":{\"device_w\":7.91},\"emi\":[{\"name\":\"RAPL\",\"raw\":123456789012}],\"flags\":[\"gap\",\"held\"],\"empty\":[],\"x\":null,\"b\":true}"));
        Run("flatten: nested object", () => Value(cells, "calc.device_w") == "7.91");
        Run("flatten: array of objects uses an index", () => Value(cells, "emi[0].name") == "RAPL");
        Run("flatten: number is kept exactly", () => Value(cells, "emi[0].raw") == "123456789012");
        Run("flatten: array of words is joined with a space", () => Value(cells, "flags") == "gap held");
        Run("flatten: empty array gives an empty cell", () => Value(cells, "empty") == "");
        Run("flatten: null and bool", () => Value(cells, "x") == "null" && Value(cells, "b") == "true");
        Run("flatten: order is the order of the line", () => cells.First().Key == "t" && cells[1].Key == "calc.device_w");
        string many = "{" + string.Join(",", Enumerable.Range(0, 500).Select(i => "\"k" + i + "\":" + i)) + "}";
        Run("flatten: at most 400 cells in one row", () => JsonFlattener.Flatten(Json(many)).Count == 400);
        string longValue = "{\"a\":\"" + new string('z', 300) + "\"}";
        Run("flatten: a long value is cut to 200", () => JsonFlattener.Flatten(Json(longValue))[0].Value.Length == 200);
        Run("flatten: a long name is cut to 200", () => JsonFlattener.Flatten(Json("{\"" + new string('n', 300) + "\":1}"))[0].Key.Length == 200);
        Run("flatten: control chars in a value are cleaned", () => JsonFlattener.Flatten(Json("{\"a\":\"x\\ny\"}"))[0].Value == "x y");
    }

    // ---------------------------------------------------------------- DiagLineParser
    private static void LineParserTests()
    {
        DiagRow? row = DiagLineParser.Parse(Tick("2026-11-22T10:31:00Z", ",\"cpu\":{\"pct\":18.6}"));
        Run("parser: valid tick", () => row is not null && row.Kind == "tick" && row.Time == new DateTimeOffset(2026, 11, 22, 10, 31, 0, TimeSpan.Zero));
        Run("parser: cells are flattened", () => Value(row!.Cells, "cpu.pct") == "18.6");
        Run("parser: missing t is skipped", () => DiagLineParser.Parse("{\"ts\":\"2026-11-22T10:31:00Z\"}") is null);
        Run("parser: missing ts is skipped", () => DiagLineParser.Parse("{\"t\":\"tick\"}") is null);
        Run("parser: t that is not text is skipped", () => DiagLineParser.Parse("{\"t\":1,\"ts\":\"2026-11-22T10:31:00Z\"}") is null);
        Run("parser: ts in a wrong form is skipped", () => DiagLineParser.Parse("{\"t\":\"tick\",\"ts\":\"yesterday\"}") is null);
        Run("parser: array is skipped", () => DiagLineParser.Parse("[1,2]") is null);
        Run("parser: garbage is skipped", () => DiagLineParser.Parse("not json") is null);
        Run("parser: half line is skipped", () => DiagLineParser.Parse("{\"t\":\"tick\",\"ts\":\"2026-11-22T10:3") is null);
        Run("parser: nesting of 6 levels is read", () => DiagLineParser.Parse("{\"t\":\"x\",\"ts\":\"2026-11-22T10:31:00Z\",\"a\":{\"b\":{\"c\":{\"d\":[1]}}}}") is not null);
        Run("parser: nesting of 7 levels is skipped", () => DiagLineParser.Parse("{\"t\":\"x\",\"ts\":\"2026-11-22T10:31:00Z\",\"a\":{\"b\":{\"c\":{\"d\":{\"e\":1}}}}}") is null);
        Run("parser: a line over 64 KB is skipped", () => DiagLineParser.Parse(Tick("2026-11-22T10:31:00Z", ",\"p\":\"" + new string('a', 70000) + "\"")) is null);
        Run("parser: a byte order mark is ignored", () => DiagLineParser.Parse("﻿" + Tick("2026-11-22T10:31:00Z")) is not null);
    }

    // ---------------------------------------------------------------- DiagTableBuilder
    private static DiagRow R(string kind, string ts, params string[] pairs)
    {
        var cells = new List<KeyValuePair<string, string>> { new("t", kind), new("ts", ts) };
        for (int i = 0; i + 1 < pairs.Length; i += 2) cells.Add(new(pairs[i], pairs[i + 1]));
        return new DiagRow(kind, DateTimeOffset.Parse(ts), cells);
    }

    private static void TableBuilderTests()
    {
        var rows = new List<DiagRow> { R("tick", "2026-11-22T10:00:00Z", "a", "1", "b", "2"), R("tick", "2026-11-22T10:01:00Z", "c", "3", "a", "4") };
        DiagTable table = DiagTableBuilder.Build(rows, "ts");
        Run("table: ts is the first column", () => table.Columns[0] == "ts");
        Run("table: union in the order of first appearance (oldest first)", () => string.Join(",", table.Columns) == "ts,t,a,b,c");
        Run("table: newest row first", () => table.Rows[0][0] == "2026-11-22T10:01:00Z");
        Run("table: a missing value is an empty cell", () => table.Rows[0][3] == "" && table.Rows[1][4] == "");
        Run("table: values land in the right column", () => table.Rows[0][2] == "4" && table.Rows[0][4] == "3" && table.Rows[1][3] == "2");
        DiagTable reports = DiagTableBuilder.Build(new[] { R("report", "2026-11-22T10:00:00Z", "report.energy_uj", "5", "queue", "3") }, "ts", "queue");
        Run("table: ts and queue first for reports", () => string.Join(",", reports.Columns.Take(3)) == "ts,queue,t");
        DiagTable sends = DiagTableBuilder.Build(new[] { R("send", "2026-11-22T10:00:00Z", "http", "201") }, "ts", "t");
        Run("table: ts and t first for sends and events", () => string.Join(",", sends.Columns) == "ts,t,http");
        DiagTable noQueue = DiagTableBuilder.Build(new[] { R("report", "2026-11-22T10:00:00Z", "x", "1") }, "ts", "queue");
        Run("table: a wanted first column that no row has is not added", () => !noQueue.Columns.Contains("queue"));
        var many = Enumerable.Range(0, 6000).Select(i => R("tick", Now.AddMinutes(-6000 + i).ToString("yyyy-MM-ddTHH:mm:ssZ"), "n", i.ToString())).ToList();
        DiagTable big = DiagTableBuilder.Build(many, "ts");
        Run("table: at most 5000 rows", () => big.Rows.Count == 5000);
        Run("table: the 5000 are the newest", () => big.Rows[0][2] == "5999" && big.Rows[4999][2] == "1000");
        var wide = new DiagRow("tick", Now, Enumerable.Range(0, 450).Select(i => new KeyValuePair<string, string>("c" + i, "v")).Prepend(new("ts", "x")).ToList());
        Run("table: at most 400 columns", () => DiagTableBuilder.Build(new[] { wide }, "ts").Columns.Count == 400);
        Run("table: ts survives the column cap", () => DiagTableBuilder.Build(new[] { wide }, "ts").Columns[0] == "ts");
        Run("table: empty input gives an empty table", () => DiagTableBuilder.Build(new List<DiagRow>(), "ts").Rows.Count == 0);
        Run("table: widths are kept between 6 and 40 characters", () => table.WidthChars.All(w => w >= 6 && w <= 40));
    }

    // ---------------------------------------------------------------- DiagFileReader
    private static string Logs(string root) => Path.Combine(root, "logs");

    private static void FileReaderTests()
    {
        string none = NewDir();
        DiagReadResult missing = DiagFileReader.Read(Logs(none), Now);
        Run("reader: no logs folder gives no diagnostics and no error", () => missing.TotalLines == 0 && !missing.FolderUnreadable && missing.Readings.Count == 0);

        string empty = NewDir();
        Put(empty, "logs/diag-2026-11-22.jsonl", "");
        Run("reader: an empty file gives no diagnostics", () => DiagFileReader.Read(Logs(empty), Now).TotalLines == 0);

        string mixed = NewDir();
        string good = Tick("2026-11-22T11:00:00Z");
        Put(mixed, "logs/diag-2026-11-22.jsonl", good + "\n" + "garbage\n" + "{\"t\":\"report\",\"ts\":\"2026-11-22T11:01:00Z\",\"queue\":3}\n" + "{\"t\":\"send\",\"ts\":\"2026-11-22T11:02:00Z\"}\n" + "{\"t\":\"event\",\"ts\":\"2026-11-22T11:03:00Z\",\"what\":\"start\"}\n" + "\n" + "{\"ts\":\"2026-11-22T11:04:00Z\"}\n");
        DiagReadResult m = DiagFileReader.Read(Logs(mixed), Now);
        Run("reader: lines go to their tables", () => m.Readings.Count == 1 && m.Reports.Count == 1 && m.SendsEvents.Count == 2);
        Run("reader: bad lines are counted, blank lines are not", () => m.SkippedLines == 2 && m.TotalLines == 6);
        Run("reader: sends and events keep the order of the file", () => m.SendsEvents[0].Kind == "send" && m.SendsEvents[1].Kind == "event");

        string half = NewDir();
        Put(half, "logs/diag-2026-11-22.jsonl", good + "\n{\"t\":\"tick\",\"ts\":\"2026-11-22T11:01:0");
        DiagReadResult h = DiagFileReader.Read(Logs(half), Now);
        Run("reader: a half written last line is ignored and not counted", () => h.Readings.Count == 1 && h.SkippedLines == 0);

        string noEnd = NewDir();
        Put(noEnd, "logs/diag-2026-11-22.jsonl", good);
        Run("reader: a complete last line without a line end is read", () => DiagFileReader.Read(Logs(noEnd), Now).Readings.Count == 1);

        string crlf = NewDir();
        Put(crlf, "logs/diag-2026-11-22.jsonl", good + "\r\n" + good + "\r\n");
        Run("reader: CRLF line ends work", () => DiagFileReader.Read(Logs(crlf), Now).Readings.Count == 2);

        string longLine = NewDir();
        Put(longLine, "logs/diag-2026-11-22.jsonl", Tick("2026-11-22T11:00:00Z", ",\"p\":\"" + new string('a', 70000) + "\"") + "\n" + good + "\n");
        DiagReadResult l = DiagFileReader.Read(Logs(longLine), Now);
        Run("reader: a line over 64 KB is skipped and counted, the next line is read", () => l.SkippedLines == 1 && l.Readings.Count == 1);

        string old = NewDir();
        Put(old, "logs/diag-2026-11-21.jsonl", Tick("2026-11-21T11:59:00Z") + "\n" + Tick("2026-11-21T12:01:00Z") + "\n");
        Run("reader: ticks older than 24 hours are dropped", () => DiagFileReader.Read(Logs(old), Now).Readings.Count == 1);

        string two = NewDir();
        Put(two, "logs/diag-2026-11-22.jsonl", "{\"t\":\"report\",\"ts\":\"2026-11-22T09:00:00Z\",\"queue\":2}\n");
        Put(two, "logs/diag-2026-11-21.jsonl", "{\"t\":\"report\",\"ts\":\"2026-11-21T09:00:00Z\",\"queue\":1}\n");
        DiagReadResult t = DiagFileReader.Read(Logs(two), Now);
        Run("reader: older file comes first, whatever the listing order", () => t.Reports.Count == 2 && t.Reports[0].Time.Day == 21);

        string names = NewDir();
        Put(names, "logs/diag-notadate.jsonl", good + "\n");
        Put(names, "logs/diag-2026-11-22.txt", good + "\n");
        Put(names, "logs/service.log", good + "\n");
        Run("reader: only diag-yyyy-MM-dd.jsonl files are read", () => DiagFileReader.Read(Logs(names), Now).TotalLines == 0);

        string big = NewDir();
        Directory.CreateDirectory(Logs(big));
        using (var fs = new FileStream(Path.Combine(Logs(big), "diag-2026-11-22.jsonl"), FileMode.Create)) fs.SetLength(DiagLimits.MaxFileBytes + 1);
        Put(big, "logs/diag-2026-11-21.jsonl", Tick("2026-11-21T13:00:00Z") + "\n");
        DiagReadResult b = DiagFileReader.Read(Logs(big), Now);
        Run("reader: a file over 64 MB is not read and is counted", () => b.SkippedLines == 1 && b.Readings.Count == 1);

        string cap = NewDir();
        var sb = new StringBuilder();
        for (int i = 0; i < 6000; i++) sb.Append("{\"t\":\"report\",\"ts\":\"2026-11-22T09:00:00Z\",\"queue\":").Append(i).Append("}\n");
        Put(cap, "logs/diag-2026-11-22.jsonl", sb.ToString());
        DiagReadResult c = DiagFileReader.Read(Logs(cap), Now);
        Run("reader: memory is bounded and the newest rows stay", () => c.Reports.Count <= 2 * DiagLimits.MaxRows && Value(c.Reports.Last().Cells, "queue") == "5999");

        string shared = NewDir();
        string sharedFile = Path.Combine(Logs(shared), "diag-2026-11-22.jsonl");
        Directory.CreateDirectory(Logs(shared));
        using var writer = new FileStream(sharedFile, FileMode.Create, FileAccess.Write, FileShare.ReadWrite | FileShare.Delete);
        byte[] bytes = Encoding.UTF8.GetBytes(good + "\n");
        writer.Write(bytes);
        writer.Flush();
        Run("reader: a file that the service still holds open for writing is read", () => DiagFileReader.Read(Logs(shared), Now).Readings.Count == 1);
        writer.Write(bytes);
        writer.Flush();
        Run("reader: the next read sees the appended line", () => DiagFileReader.Read(Logs(shared), Now).Readings.Count == 2);
    }

    // ---------------------------------------------------------------- JsonFileRows
    private static void JsonFileRowsTests()
    {
        string dir = NewDir();
        Run("json rows: a missing file is unknown", () => !JsonFileRows.Read(Path.Combine(dir, "nope.json")).Available);
        Put(dir, "a.json", "{\"schema\":1,\"level\":\"L2\",\"sources\":[{\"id\":\"emi\",\"channels\":[{\"name\":\"RAPL\"}]}],\"queued\":null}");
        JsonFileResult a = JsonFileRows.Read(Path.Combine(dir, "a.json"));
        Run("json rows: a good file is available", () => a.Available);
        Run("json rows: names are flattened in file order", () => a.Rows[0].Name == "schema" && a.Rows[2].Name == "sources[0].id" && a.Rows[3].Name == "sources[0].channels[0].name");
        Run("json rows: values are kept", () => a.Rows[1].Value == "L2" && a.Rows.Last().Value == "null");
        Put(dir, "bad.json", "{ not json");
        Run("json rows: a broken file is unknown, not empty", () => !JsonFileRows.Read(Path.Combine(dir, "bad.json")).Available);
        Put(dir, "arr.json", "[1,2]");
        Run("json rows: a file that is not an object is unknown", () => !JsonFileRows.Read(Path.Combine(dir, "arr.json")).Available);
        Put(dir, "deep.json", "{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":{\"f\":1}}}}}}");
        Run("json rows: nesting of 7 levels is unknown", () => !JsonFileRows.Read(Path.Combine(dir, "deep.json")).Available);
        Put(dir, "empty.json", "");
        Run("json rows: an empty file is unknown", () => !JsonFileRows.Read(Path.Combine(dir, "empty.json")).Available);
        using (var fs = new FileStream(Path.Combine(dir, "huge.json"), FileMode.Create)) fs.SetLength(DiagLimits.MaxFileBytes + 1);
        Run("json rows: a file over 64 MB is unknown", () => !JsonFileRows.Read(Path.Combine(dir, "huge.json")).Available);
        string manyRows = "{" + string.Join(",", Enumerable.Range(0, 450).Select(i => "\"k" + i + "\":1")) + "}";
        Put(dir, "many.json", manyRows);
        Run("json rows: the cell cap of a row holds", () => JsonFileRows.Read(Path.Combine(dir, "many.json")).Rows.Count == 400);
    }

    // ---------------------------------------------------------------- LogTailReader
    private static void LogTailTests()
    {
        string dir = NewDir();
        Run("log: a missing file is unavailable", () => !LogTailReader.Read(Path.Combine(dir, "service.log")).Available);
        Put(dir, "empty.log", "");
        LogTailResult e = LogTailReader.Read(Path.Combine(dir, "empty.log"));
        Run("log: an empty file is available with no lines", () => e.Available && e.Lines.Count == 0);
        Put(dir, "five.log", "one\ntwo\r\nthree\n\nfour\nfive");
        LogTailResult f = LogTailReader.Read(Path.Combine(dir, "five.log"));
        Run("log: newest first, blank lines and CR removed, last line without end kept", () => string.Join("|", f.Lines) == "five|four|three|two|one");
        Put(dir, "many.log", string.Join("\n", Enumerable.Range(1, 500).Select(i => "line " + i)) + "\n");
        LogTailResult m = LogTailReader.Read(Path.Combine(dir, "many.log"));
        Run("log: the last 300 lines", () => m.Lines.Count == 300 && m.Lines[0] == "line 500" && m.Lines[299] == "line 201");
        Put(dir, "ctl.log", "a\u0001b\n");
        Run("log: control characters are cleaned", () => LogTailReader.Read(Path.Combine(dir, "ctl.log")).Lines[0] == "a b");
        Put(dir, "long.log", "ok\n" + new string('q', 70000) + "\nlast\n");
        LogTailResult lg = LogTailReader.Read(Path.Combine(dir, "long.log"));
        Run("log: a line over 64 KB is skipped and counted", () => lg.SkippedLines == 1 && lg.Lines.Count == 2);
        string bigText = string.Join("\n", Enumerable.Range(1, 40000).Select(i => "line number " + i)) + "\n";
        Put(dir, "huge.log", bigText);
        LogTailResult hg = LogTailReader.Read(Path.Combine(dir, "huge.log"));
        Run("log: a file over 1 MB is read from its end and the cut first line is dropped", () => hg.Lines[0] == "line number 40000" && hg.Lines.Count == 300);
    }

    // ---------------------------------------------------------------- DeveloperModeFile
    private static void DeveloperModeTests()
    {
        string dir = NewDir();
        string path = Path.Combine(dir, "sub", "developer-mode.txt");
        Run("mode: no file is off", () => !DeveloperModeFile.IsOn(path));
        Run("mode: switching on writes 1 and makes the folder", () => DeveloperModeFile.TrySet(path, true) && File.ReadAllText(path).Trim() == "1");
        Run("mode: the file reads as on", () => DeveloperModeFile.IsOn(path));
        Run("mode: switching off removes the file", () => DeveloperModeFile.TrySet(path, false) && !File.Exists(path));
        Run("mode: switching off with no file is fine", () => DeveloperModeFile.TrySet(path, false));
        Put(dir, "n.txt", "1\r\n");
        Run("mode: 1 with a line end is on", () => DeveloperModeFile.IsOn(Path.Combine(dir, "n.txt")));
        Put(dir, "z.txt", "0");
        Run("mode: 0 is off", () => !DeveloperModeFile.IsOn(Path.Combine(dir, "z.txt")));
        Put(dir, "g.txt", "yes please");
        Run("mode: other text is off", () => !DeveloperModeFile.IsOn(Path.Combine(dir, "g.txt")));
        Put(dir, "big.txt", "1" + new string(' ', 5000) + "x");
        Run("mode: a big file is off", () => !DeveloperModeFile.IsOn(Path.Combine(dir, "big.txt")));
        Run("mode: a path under a file cannot be written", () => !DeveloperModeFile.TrySet(Path.Combine(dir, "z.txt", "x", "m.txt"), true));
        Run("mode: the default path is in LocalApplicationData", () => DeveloperModeFile.DefaultPath.EndsWith(Path.Combine("EcoWattSense", "developer-mode.txt")));
    }

    // ---------------------------------------------------------------- StatusMasker
    private static void StatusMaskerTests()
    {
        string text = "{\"schema\":1,\"level\":\"L2\",\"server_url\":\"https://x\",\"device_name\":\"PC-1\",\"server_device_name\":null,\"queued\":0,\"big\":123456789012345678}";
        byte[]? masked = StatusMasker.Mask(Encoding.UTF8.GetBytes(text));
        Run("mask: gives a result for an object", () => masked is not null);
        string outText = Encoding.UTF8.GetString(masked!);
        Run("mask: the three fields are gone", () => !outText.Contains("server_url") && !outText.Contains("device_name") && !outText.Contains("server_device_name") && !outText.Contains("PC-1") && !outText.Contains("https://x"));
        Run("mask: the other fields stay with exact numbers", () => outText.Contains("\"level\":\"L2\"") && outText.Contains("123456789012345678") && outText.Contains("\"queued\":0"));
        Run("mask: the result is JSON", () => JsonDocument.Parse(masked!).RootElement.ValueKind == JsonValueKind.Object);
        Run("mask: the names match in any case", () => !Encoding.UTF8.GetString(StatusMasker.Mask(Encoding.UTF8.GetBytes("{\"Server_URL\":\"x\",\"a\":1}"))!).Contains("Server_URL"));
        Run("mask: broken JSON gives null", () => StatusMasker.Mask(Encoding.UTF8.GetBytes("{ nope")) is null);
        Run("mask: a JSON array gives null", () => StatusMasker.Mask(Encoding.UTF8.GetBytes("[1]")) is null);
        Run("mask: empty input gives null", () => StatusMasker.Mask(Array.Empty<byte>()) is null);
    }

    // ---------------------------------------------------------------- DiagExportPlan
    private static string FullDataFolder()
    {
        string d = NewDir();
        string[] files =
        {
            "scan/capabilities.json", "scan/capabilities.service.json", "scan/scan.log", "scan/scan.service.log",
            "history/2026-11-21.csv", "history/2026-11-22.csv", "history/notes.txt", "history/sub/2026-11-20.csv",
            "logs/service.log", "logs/service.previous.log", "logs/diag-2026-11-21.jsonl", "logs/diag-2026-11-22.jsonl",
            "logs/diag-old.txt", "logs/install.log", "calibration/model.json", "calibration/model.previous.json",
            "config/service.ini", "config/key.bin", "queue/reports.jsonl", "Client/device-id.txt", "Client/instance.lock"
        };
        foreach (string f in files) Put(d, f, "content of " + f);
        Put(d, "status/status.json", "{\"schema\":1,\"server_url\":\"https://secret\",\"device_name\":\"HOME-PC\",\"server_device_name\":\"X\",\"level\":\"L2\"}");
        return d;
    }

    private static void ExportPlanTests()
    {
        string d = FullDataFolder();
        ExportPlan plan = DiagExportPlan.Build(d);
        var names = plan.Entries.Select(e => e.ZipPath).OrderBy(x => x, StringComparer.Ordinal).ToList();
        string[] expected =
        {
            "calibration/model.json", "history/2026-11-21.csv", "history/2026-11-22.csv", "logs/diag-2026-11-21.jsonl", "logs/diag-2026-11-22.jsonl",
            "logs/service.log", "logs/service.previous.log", "scan/capabilities.json", "scan/capabilities.service.json", "scan/scan.log", "scan/scan.service.log",
            "status/status.json"
        };
        Run("plan: exactly the allowed files", () => names.SequenceEqual(expected.OrderBy(x => x, StringComparer.Ordinal)));
        Run("plan: nothing from config, queue or Client", () => plan.Entries.All(e => !e.ZipPath.StartsWith("config") && !e.ZipPath.StartsWith("queue") && !e.ZipPath.StartsWith("Client")));
        Run("plan: no source file from config, queue or Client", () => plan.Entries.All(e => e.SourcePath is null || !(e.SourcePath.Contains("config") || e.SourcePath.Contains("queue") || e.SourcePath.Contains("Client"))));
        Run("plan: install.log is not in", () => plan.Entries.All(e => !e.ZipPath.Contains("install")));
        Run("plan: model.previous.json is not in", () => plan.Entries.All(e => !e.ZipPath.Contains("previous.json")));
        Run("plan: no ZIP path has a backslash or a dot dot", () => plan.Entries.All(e => !e.ZipPath.Contains('\\') && !e.ZipPath.Contains("..")));
        ExportEntry status = plan.Entries.Single(e => e.ZipPath == "status/status.json");
        string statusText = Encoding.UTF8.GetString(status.Content!);
        Run("plan: status is masked content, not a file copy", () => status.SourcePath is null && !statusText.Contains("secret") && !statusText.Contains("HOME-PC") && statusText.Contains("\"level\":\"L2\""));
        Run("plan: nothing is missing", () => plan.MissingCount == 0);
        File.Delete(Path.Combine(d, "scan", "scan.service.log"));
        File.Delete(Path.Combine(d, "calibration", "model.json"));
        File.WriteAllText(Path.Combine(d, "status", "status.json"), "{ broken");
        ExportPlan fewer = DiagExportPlan.Build(d);
        Run("plan: missing files are counted, a status that cannot be masked is left out", () => fewer.MissingCount == 3 && fewer.Entries.All(e => e.ZipPath != "status/status.json"));
        Run("plan: an empty data folder gives no entries", () => DiagExportPlan.Build(NewDir()).Entries.Count == 0 && DiagExportPlan.Build(NewDir()).MissingCount == 9);
    }

    // ---------------------------------------------------------------- DiagZipWriter
    private static void ZipWriterTests()
    {
        string d = FullDataFolder();
        string target = Path.Combine(NewDir(), "out.zip");
        ExportPlan plan = DiagExportPlan.Build(d);
        ExportResult result = DiagZipWriter.Write(plan, target);
        Run("zip: saved with all files", () => result.Saved && result.Written == 12 && result.LeftOut == 0);
        using ZipArchive zip = ZipFile.OpenRead(target);
        var entries = zip.Entries.Select(e => e.FullName).ToList();
        Run("zip: has the notes file", () => entries.Contains("export-notes.txt"));
        Run("zip: has 12 files and the notes", () => entries.Count == 13);
        Run("zip: entry names use a slash", () => entries.All(n => !n.Contains('\\')));
        string notes = new StreamReader(zip.GetEntry("export-notes.txt")!.Open()).ReadToEnd();
        Run("zip: notes hold fixed words and counts", () => notes.Contains("files_written: 12") && notes.Contains("files_left_out: 0"));
        string statusInZip = new StreamReader(zip.GetEntry("status/status.json")!.Open()).ReadToEnd();
        Run("zip: status in the ZIP is masked", () => !statusInZip.Contains("secret") && !statusInZip.Contains("HOME-PC"));
        string copied = new StreamReader(zip.GetEntry("logs/service.log")!.Open()).ReadToEnd();
        Run("zip: a copied file has its content", () => copied == "content of logs/service.log");

        string d2 = FullDataFolder();
        ExportPlan plan2 = DiagExportPlan.Build(d2);
        File.Delete(Path.Combine(d2, "logs", "service.previous.log"));
        string target2 = Path.Combine(NewDir(), "out2.zip");
        ExportResult r2 = DiagZipWriter.Write(plan2, target2);
        using ZipArchive zip2 = ZipFile.OpenRead(target2);
        string notes2 = new StreamReader(zip2.GetEntry("export-notes.txt")!.Open()).ReadToEnd();
        Run("zip: a source that disappeared is left out and counted", () => r2.Saved && r2.Written == 11 && r2.LeftOut == 1 && zip2.GetEntry("logs/service.previous.log") is null && notes2.Contains("files_left_out: 1"));

        ExportPlan missingPlan = DiagExportPlan.Build(NewDir());
        string target3 = Path.Combine(NewDir(), "none.zip");
        ExportResult r3 = DiagZipWriter.Write(missingPlan, target3);
        Run("zip: with no file at all the export fails and leaves no file", () => !r3.Saved && !File.Exists(target3));

        string dirTarget = NewDir();
        ExportResult r4 = DiagZipWriter.Write(plan, dirTarget);
        Run("zip: a target that cannot be written fails", () => !r4.Saved);
        string noFolder = Path.Combine(NewDir(), "missing-folder", "x.zip");
        Run("zip: a target in a missing folder fails", () => !DiagZipWriter.Write(plan, noFolder).Saved);

        string d5 = FullDataFolder();
        ExportPlan plan5 = DiagExportPlan.Build(d5);
        string target5 = Path.Combine(NewDir(), "twice.zip");
        File.WriteAllText(target5, "old");
        Run("zip: an existing target is replaced", () => DiagZipWriter.Write(plan5, target5).Saved && ZipFile.OpenRead(target5).Entries.Count == 13);
    }

    // ---------------------------------------------------------------- DiagText
    private static void TextTests()
    {
        Run("text: N rows", () => DiagText.CountLine(5, 0, false, false) == "5 rows");
        Run("text: one row", () => DiagText.CountLine(1, 0, false, false) == "1 row");
        Run("text: rows and skipped lines", () => DiagText.CountLine(5, 2, false, false) == "5 rows. 2 lines could not be read.");
        Run("text: one skipped line", () => DiagText.CountLine(0, 1, false, false) == "0 rows. 1 line could not be read.");
        Run("text: no diagnostics yet", () => DiagText.CountLine(0, 0, true, false) == "No diagnostics yet. The service writes one line each minute.");
        Run("text: unreadable folder", () => DiagText.CountLine(0, 0, true, true) == "The diagnostics folder could not be read.");
        Run("text: export file name", () => DiagText.ExportFileName(new DateTime(2026, 11, 22, 10, 31, 59)) == "EcoWattSense-diagnostics-20261122-1031.zip");
    }

    // ---------------------------------------------------------------- DiagSnapshotLoader
    private static void SnapshotTests()
    {
        string d = NewDir();
        Put(d, "logs/diag-2026-11-22.jsonl", Tick("2026-11-22T11:00:00Z", ",\"calc\":{\"device_w\":7.9}") + "\n" + "{\"t\":\"report\",\"ts\":\"2026-11-22T11:00:01Z\",\"queue\":1,\"report\":{\"energy_uj\":5}}\n" + "{\"t\":\"event\",\"ts\":\"2026-11-22T11:00:02Z\",\"what\":\"start\"}\n");
        Put(d, "logs/service.log", "start\nstop\n");
        Put(d, "status/status.json", "{\"level\":\"L2\"}");
        Put(d, "scan/capabilities.json", "{\"level\":\"L1\"}");
        DiagSnapshot s = DiagSnapshotLoader.Load(d, Now);
        Run("snapshot: readings table", () => s.Readings.Rows.Count == 1 && s.Readings.Columns[0] == "ts" && s.Readings.Columns.Contains("calc.device_w"));
        Run("snapshot: reports table has ts and queue first", () => s.Reports.Columns[0] == "ts" && s.Reports.Columns[1] == "queue" && s.Reports.Columns.Contains("report.energy_uj"));
        Run("snapshot: sends and events table has ts and t first", () => s.SendsEvents.Columns[0] == "ts" && s.SendsEvents.Columns[1] == "t");
        Run("snapshot: status, capabilities and log", () => s.Status.Available && s.Capabilities.Available && !s.CapabilitiesService.Available && s.Log.Available && s.Log.Lines[0] == "stop");
        DiagSnapshot nothing = DiagSnapshotLoader.Load(NewDir(), Now);
        Run("snapshot: an empty data folder is all unknown and no error", () => !nothing.Status.Available && !nothing.Log.Available && nothing.Counts.TotalLines == 0 && nothing.Readings.Rows.Count == 0);
    }
}
