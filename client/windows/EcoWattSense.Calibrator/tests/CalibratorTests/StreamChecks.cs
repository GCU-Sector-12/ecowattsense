// StreamChecks.cs: checks of the line parser, the bounded line reader and the stream session (spec 1.6 and 2.1): good lines, bad lines,
// limits, and the rule that a bad line is skipped and counted.
// Who uses it: Program.Main.
using EnergyCalibrator;
using static CalibratorTests.Harness;

namespace CalibratorTests;

/// <summary>Checks of the stream text.</summary>
internal static class StreamChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Group("header", HeaderChecks);
        Group("header refused", HeaderRefusedChecks);
        Group("header cleaning", HeaderCleaningChecks);
        Group("sample", SampleChecks);
        Group("sample refused", SampleRefusedChecks);
        Group("line reader", LineReaderChecks);
        Group("session", SessionChecks);
    }

    /// <summary>The example header of the spec is read completely.</summary>
    private static void HeaderChecks()
    {
        WatchHeader? h = WatchParser.ParseHeader(StreamData.HeaderLine, out string reason);
        Check(h != null && reason == "", "header: the spec example is accepted");
        if (h == null) { return; }
        Check(h.PeriodMs == 2000, "header: period_ms is read");
        Check(h.Emi.Count == 2 && h.Emi[0].Name == "RAPL_Package0_PKG" && h.Emi[0].Kind == "cpu_package", "header: the package channel is read with its kind");
        Check(h.Emi[1].Kind == "cpu_core" && h.Emi[1].Index == 1, "header: the core channel has the kind cpu_core");
        Check(h.Nvml.Count == 1 && h.Nvml[0].Adapter == 1 && h.Nvml[0].Power && !h.Nvml[0].Energy, "header: the NVML device is read");
        Check(h.GpuUse.Count == 2 && h.GpuUse[1].Role == "high_performance" && h.GpuUse[0].VendorId == 4098, "header: the adapters are read");
        Check(h.Machine == new WatchMachine("AuthenticAMD", 25, 80, 8), "header: the machine fields are read");
        Check(h.Battery == new WatchBattery(50000, 45000, 120), "header: the battery fields are read");
        Check(h.Display == new WatchDisplay(2, 1, 60), "header: the display fields are read");
    }

    /// <summary>Headers that must be refused, each with its fixed reason word.</summary>
    private static void HeaderRefusedChecks()
    {
        Refused("not json", "this is not json", WatchReasons.BadJson);
        Refused("a json array", "[1,2]", WatchReasons.BadJson);
        Refused("no watch field", "{\"period_ms\":2000,\"emi\":[]}", WatchReasons.NotHeader);
        Refused("watch 2", "{\"watch\":2,\"period_ms\":2000,\"emi\":[]}", WatchReasons.BadVersion);
        Refused("watch as text", "{\"watch\":\"1\",\"period_ms\":2000,\"emi\":[]}", WatchReasons.BadVersion);
        Refused("period too small", "{\"watch\":1,\"period_ms\":100,\"emi\":[]}", WatchReasons.BadValue);
        Refused("period too big", "{\"watch\":1,\"period_ms\":20000,\"emi\":[]}", WatchReasons.BadValue);
        Refused("no emi list", "{\"watch\":1,\"period_ms\":2000}", WatchReasons.BadValue);
        Refused("emi entry without name", "{\"watch\":1,\"period_ms\":2000,\"emi\":[{\"kind\":\"dram\"}]}", WatchReasons.BadValue);
        string many = string.Join(",", Enumerable.Repeat("{\"name\":\"a\",\"kind\":\"other\",\"device\":0,\"index\":0}", 257));
        Refused("257 channels", "{\"watch\":1,\"period_ms\":2000,\"emi\":[" + many + "]}", WatchReasons.BadLength);
        Refused("line over 64 KB", "{\"watch\":1,\"period_ms\":2000,\"emi\":[],\"pad\":\"" + StreamData.Letters(70_000) + "\"}", WatchReasons.TooLong);
        string deep = new string('[', 40) + new string(']', 40);
        Refused("json nested too deep", "{\"watch\":1,\"period_ms\":2000,\"emi\":[],\"deep\":" + deep + "}", WatchReasons.BadJson);
    }

    /// <summary>Checks that a header is refused with a reason.</summary>
    /// <param name="name">What is checked.</param>
    /// <param name="line">The line.</param>
    /// <param name="expected">The reason word.</param>
    private static void Refused(string name, string line, string expected)
    {
        WatchHeader? h = WatchParser.ParseHeader(line, out string reason);
        Check(h == null && reason == expected, $"header refused: {name} gives {expected}");
    }

    /// <summary>Odd names, odd kinds and odd context numbers are cleaned or left out; the header stays good.</summary>
    private static void HeaderCleaningChecks()
    {
        string line = "{\"watch\":1,\"period_ms\":2000,\"emi\":[{\"name\":\"" + StreamData.Letters(300) + "\",\"kind\":\"weird kind\",\"device\":0,\"index\":0},"
            + "{\"name\":\"bad\\u0001name\\n\",\"kind\":\"dram\",\"device\":1,\"index\":2}],"
            + "\"machine\":{\"cpu_vendor\":\"Vendor<script>\",\"cpu_family\":99999,\"cpu_model\":-3,\"physical_cores\":\"eight\"},"
            + "\"battery\":{\"design_mwh\":-5,\"full_mwh\":1e12,\"cycle_count\":70000},\"display\":{\"displays\":2,\"external_displays\":500,\"refresh_hz\":0}}";
        WatchHeader? h = WatchParser.ParseHeader(line, out _);
        Check(h != null, "cleaning: a header with odd values is still accepted");
        if (h == null) { return; }
        Check(h.Emi[0].Name.Length == 128, "cleaning: a long channel name is cut to 128 characters");
        Check(h.Emi[0].Kind == "other", "cleaning: an unknown kind becomes other");
        Check(h.Emi[1].Name == "badname", "cleaning: control characters are removed from a name");
        Check(h.Machine.CpuVendor == "Vendorscript", "cleaning: odd signs are removed from the vendor");
        Check(h.Machine.CpuFamily == null && h.Machine.CpuModel == null && h.Machine.PhysicalCores == null, "cleaning: out of range or wrong type machine numbers are left out");
        Check(h.Battery.DesignMwh == null && h.Battery.FullMwh == null && h.Battery.CycleCount == 70000, "cleaning: out of range battery numbers are left out, a believable cycle count stays");
        Check(h.Display.Displays == 2 && h.Display.ExternalDisplays == null && h.Display.RefreshHz == null, "cleaning: out of range display numbers are left out");
    }

    /// <summary>The example line of the spec is read, null entries stay null.</summary>
    private static void SampleChecks()
    {
        WatchHeader header = StreamData.Header();
        WatchSample? s = WatchParser.ParseSample(StreamData.SampleLine, header, 2000, out string reason);
        Check(s != null && reason == "", "sample: the spec example is accepted");
        if (s == null) { return; }
        Check(s.TimeMs == 4000, "sample: t_ms is read");
        Check(s.EmiUj.Count == 2 && s.EmiUj[0] == 123456 && s.EmiUj[1] == null, "sample: emi_uj has the value and the null");
        Check(s.NvmlMw.Count == 1 && s.NvmlMw[0] == 15300, "sample: nvml_mw is read");
        Check(s.NvmlMj.Count == 1 && s.NvmlMj[0] == null, "sample: nvml_mj null stays null");
        Check(s.GpuPct.Count == 2 && s.GpuPct[0] == 12.5 && s.GpuPct[1] == 0.0, "sample: gpu_pct is read");
        WatchSample? withEnergy = WatchParser.ParseSample("{\"t_ms\":6000,\"emi_uj\":[1,2],\"nvml_mw\":[1],\"nvml_mj\":[777],\"gpu_pct\":[1,1],\"unknown\":true}", header, 4000, out _);
        Check(withEnergy != null && withEnergy.NvmlMj[0] == null, "sample: nvml_mj is unknown when the header says energy is false; unknown fields are ignored");
    }

    /// <summary>Sample lines that must be refused.</summary>
    private static void SampleRefusedChecks()
    {
        WatchHeader header = StreamData.Header();
        SampleRefused(header, "not json", "{{{", WatchReasons.BadJson);
        SampleRefused(header, "emi list too short", "{\"t_ms\":6000,\"emi_uj\":[1],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", WatchReasons.BadLength);
        SampleRefused(header, "gpu list too long", "{\"t_ms\":6000,\"emi_uj\":[1,2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1,1]}", WatchReasons.BadLength);
        SampleRefused(header, "a list is missing", "{\"t_ms\":6000,\"emi_uj\":[1,2],\"nvml_mw\":[1],\"nvml_mj\":[null]}", WatchReasons.BadValue);
        SampleRefused(header, "text instead of a number", "{\"t_ms\":6000,\"emi_uj\":[\"x\",2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", WatchReasons.BadValue);
        SampleRefused(header, "negative energy", "{\"t_ms\":6000,\"emi_uj\":[-1,2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", WatchReasons.BadValue);
        SampleRefused(header, "energy with decimals", "{\"t_ms\":6000,\"emi_uj\":[1.5,2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", WatchReasons.BadValue);
        SampleRefused(header, "energy too big", "{\"t_ms\":6000,\"emi_uj\":[1e30,2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", WatchReasons.BadValue);
        SampleRefused(header, "use above 100", "{\"t_ms\":6000,\"emi_uj\":[1,2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[100.5,1]}", WatchReasons.BadValue);
        SampleRefused(header, "negative power", "{\"t_ms\":6000,\"emi_uj\":[1,2],\"nvml_mw\":[-3],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", WatchReasons.BadValue);
        SampleRefused(header, "negative time", "{\"t_ms\":-5,\"emi_uj\":[1,2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", WatchReasons.BadValue);
        SampleRefused(header, "time goes back", "{\"t_ms\":1000,\"emi_uj\":[1,2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", WatchReasons.BadTime);
        SampleRefused(header, "a second header", StreamData.HeaderLine, WatchReasons.SecondHeader);
        SampleRefused(header, "over 64 KB", "{\"t_ms\":6000,\"emi_uj\":[1,2],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1],\"pad\":\"" + StreamData.Letters(70_000) + "\"}", WatchReasons.TooLong);
    }

    /// <summary>Checks that a sample line is refused with a reason (the previous line had the time 2000).</summary>
    /// <param name="header">The header.</param>
    /// <param name="name">What is checked.</param>
    /// <param name="line">The line.</param>
    /// <param name="expected">The reason word.</param>
    private static void SampleRefused(WatchHeader header, string name, string line, string expected)
    {
        WatchSample? s = WatchParser.ParseSample(line, header, 2000, out string reason);
        Check(s == null && reason == expected, $"sample refused: {name} gives {expected}");
    }

    /// <summary>The bounded reader splits lines, handles a missing last line break, and throws a too long line away without keeping it.</summary>
    private static void LineReaderChecks()
    {
        BoundedLineReader reader = new(StreamData.Reader("one", "two\r", StreamData.Letters(300), "three"), 100);
        WatchLine? a = reader.ReadAsync(CancellationToken.None).Result;
        WatchLine? b = reader.ReadAsync(CancellationToken.None).Result;
        WatchLine? c = reader.ReadAsync(CancellationToken.None).Result;
        WatchLine? d = reader.ReadAsync(CancellationToken.None).Result;
        WatchLine? e = reader.ReadAsync(CancellationToken.None).Result;
        Check(a?.Text == "one" && b?.Text == "two", "line reader: lines are split and a carriage return is removed");
        Check(c is { TooLong: true, Text: null }, "line reader: a line over the limit is reported as too long and not kept");
        Check(d?.Text == "three" && d?.TooLong == false, "line reader: the line after a too long one is read normally");
        Check(e == null, "line reader: the end of the text gives null");
        BoundedLineReader multibyte = new(new StringReader(new string('€', 40) + "\n"), 100);
        Check(multibyte.ReadAsync(CancellationToken.None).Result is { TooLong: true }, "line reader: the limit counts UTF-8 bytes, not characters");
        BoundedLineReader empty = new(new StringReader(""), 100);
        Check(empty.ReadAsync(CancellationToken.None).Result == null, "line reader: an empty text gives null");
    }

    /// <summary>The session keeps the latest good line, skips and counts bad ones and ends on a bad or a second header.</summary>
    private static void SessionChecks()
    {
        WatchSession s = new();
        Check(s.Accept(new WatchLine(StreamData.HeaderLine, false), 10) && s.Header != null, "session: a good first line gives the header");
        Check(s.Accept(new WatchLine("{\"t_ms\":2000,\"emi_uj\":[10,null],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", false), 12), "session: a good line is accepted");
        Check(s.Latest?.TimeMs == 2000 && s.LatestSeconds == 12, "session: the latest line and its arrival time are kept");
        Check(s.Accept(new WatchLine("garbage", false), 13) && s.BadLines == 1, "session: a bad line is skipped and counted, the reading goes on");
        Check(s.Accept(new WatchLine(null, true), 13) && s.BadLines == 2, "session: a too long line is skipped and counted");
        Check(s.Accept(new WatchLine("{\"t_ms\":1500,\"emi_uj\":[10,null],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", false), 14) && s.BadLines == 3, "session: a line whose time goes back is skipped and counted");
        Check(s.Latest?.TimeMs == 2000, "session: bad lines do not replace the latest line");
        Check(s.Fresh(13, 6)?.TimeMs == 2000 && s.Fresh(20, 6) == null, "session: a line older than the limit is stale and not given");
        Check(!s.Accept(new WatchLine(StreamData.HeaderLine, false), 15) && s.End == WatchEnd.SecondHeader, "session: a second header ends the reading");
        Check(!s.Accept(new WatchLine("{\"t_ms\":9000,\"emi_uj\":[10,null],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}", false), 16) && s.Latest?.TimeMs == 2000, "session: after the end no line is taken");
        WatchSession noHeader = new();
        Check(!noHeader.Accept(new WatchLine(StreamData.SampleLine, false), 1) && noHeader.End == WatchEnd.BadHeader && noHeader.Header == null, "session: a first line that is not a header ends the reading");
        WatchSession ended = new();
        ended.Accept(new WatchLine(StreamData.HeaderLine, false), 1);
        ended.Finish(WatchEnd.InputEnded);
        ended.Finish(WatchEnd.Stopped);
        Check(ended.End == WatchEnd.InputEnded, "session: the first reason for the end is kept");
        WatchSession flood = new();
        flood.Accept(new WatchLine(StreamData.HeaderLine, false), 1);
        for (int i = 0; i < 5000; i++) { flood.Accept(new WatchLine("x", false), 2); }
        Check(flood.BadLines == 5000 && flood.End == WatchEnd.None, "session: thousands of bad lines are only counted");
    }
}
