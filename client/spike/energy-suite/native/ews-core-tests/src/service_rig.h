// service_rig.h: fakes and a ready-made rig for the tests of ServiceCore (ews/service_core.h).
// Used by test_service_core.cpp. The rig owns a temporary data folder `ews-svc-test-<name>-<pid>` below the system
// temporary folder and removes it in its destructor, so no test leaves a folder behind. Nothing here waits or sends.
#pragma once

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "ews/secret.h"
#include "ews/service_core.h"
#include "fake_sources.h"
#include "temp_folder.h"
#include "test_calc_common.h"

namespace ews_test {

/// A clock that moves only when the test says so. It starts at 2026-11-14T10:00:00Z.
class FakeClock final : public ews::IClock {
public:
    ews::UtcTime now = ews::UtcTime(std::chrono::seconds(kTestTime - 300));
    ews::UtcTime Now() override { return now; }
    /// Moves the clock forward.
    /// @param seconds Whole seconds.
    void Advance(int seconds) { now += std::chrono::seconds(seconds); }
};

/// A power source with a value the test sets.
class FakePower final : public ews::IPowerSource {
public:
    ews::Reading<bool> value = ews::Reading<bool>::Success(false);
    ews::Reading<bool> OnBattery() override { return value; }
    /// Sets mains (false) or battery (true).
    void Set(bool onBattery) { value = ews::Reading<bool>::Success(onBattery); }
};

/// A scan runner that gives one canned result.
class FakeScan final : public ews::IScanRunner {
public:
    ews::ScanResult result;
    int runs = 0;
    FakeScan() { result.level = ews::MeasurementLevel::L2; }
    ews::ScanResult Run(ews::ScanContext context) override {
        ++runs;
        result.context = context;
        result.scannedUtc = "2026-11-14T10:00:00Z";
        return result;
    }
};

/// An EMI source with a package and a memory channel whose counters grow by a fixed step at each Read.
/// The default steps are 5 W and 0.5 W over 60 seconds.
class CountingEmi final : public ews::IEmiSource {
public:
    std::vector<ews::EmiChannel> channels;
    std::vector<std::uint64_t> counters{1000000000000ull, 2000000000000ull};
    std::vector<std::uint64_t> steps{83333333333ull, 8333333333ull};
    bool failRead = false;
    CountingEmi() { channels = {MakeChannel(0, 0, "RAPL_Package0_PKG"), MakeChannel(0, 1, "RAPL_Package0_DRAM")}; }
    ews::Reading<std::vector<ews::EmiChannel>> Open() override { return ews::Reading<std::vector<ews::EmiChannel>>::Success(channels); }
    ews::Reading<std::vector<ews::EmiSample>> Read() override {
        if (failRead) {
            return ews::Reading<std::vector<ews::EmiSample>>::Failure(ews::Status::Error, 31);
        }
        std::vector<ews::EmiSample> samples;
        for (std::size_t i = 0; i < counters.size(); ++i) {
            counters[i] += steps[i];
            samples.push_back(MakeSample(counters[i], counters[i]));
        }
        return ews::Reading<std::vector<ews::EmiSample>>::Success(samples);
    }
};

/// A processor times source: each Read adds 1000 ticks, `percent` of them busy.
class FakeCpu final : public ews::ICpuTimesSource {
public:
    int percent = 20;
    bool fail = false;
    ews::CpuTimes times{1000, 2000, 500};
    ews::Reading<ews::CpuTimes> Read() override {
        if (fail) {
            return ews::Reading<ews::CpuTimes>::Failure(ews::Status::Error, 5);
        }
        std::uint64_t busy = static_cast<std::uint64_t>(percent) * 10;
        std::uint64_t idle = 1000 - busy;
        times.idle += idle;
        times.kernel += idle;
        times.user += busy;
        return ews::Reading<ews::CpuTimes>::Success(times);
    }
};

/// A sender factory whose senders record every batch and answer from a script (Ok when the script is empty).
class FakeSenders final : public ews::ISenderFactory {
public:
    std::vector<ews::SendResult> script;
    std::vector<std::vector<std::string>> batches;
    int created = 0;
    std::string lastUrl;

    /// Gives the next scripted answer and records the batch.
    ews::SendResult Next(const std::vector<std::string>& lines) {
        batches.push_back(lines);
        ews::SendResult answer = Answer(ews::SendWord::Ok, true);
        if (!script.empty()) {
            answer = script.front();
            script.erase(script.begin());
        }
        return answer;
    }

    /// Builds an answer. @param word The word. @param leave Whether the lines leave the queue. @return The result.
    static ews::SendResult Answer(ews::SendWord word, bool leave) {
        ews::SendResult result;
        result.word = word;
        result.leaveQueue = leave;
        return result;
    }

    std::unique_ptr<ews::ReportSender> Create(const std::string& url, const std::string&) override {
        ++created;
        lastUrl = url;
        return std::make_unique<Forward>(*this);
    }

private:
    /// A sender that hands every batch to the factory.
    class Forward final : public ews::ReportSender {
    public:
        explicit Forward(FakeSenders& owner) : owner_(owner) {}
        ews::SendResult Send(const std::vector<std::string>& lines) override { return owner_.Next(lines); }

    private:
        FakeSenders& owner_;
    };
};

/// A temporary data folder that is removed at the end.
class TempData {
public:
    explicit TempData(const char* name) {
        path_ = std::filesystem::temp_directory_path() / (std::string("ews-svc-test-") + name + "-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directory(path_);
    }
    ~TempData() { RemoveDataFolder(path_); }
    TempData(const TempData&) = delete;
    TempData& operator=(const TempData&) = delete;
    /// @return The folder.
    const std::filesystem::path& path() const { return path_; }
    /// @return The folder as text for the library functions.
    std::wstring wide() const { return path_.wstring(); }
    /// @return The text of a file below the folder, or "" when it is missing.
    std::string Text(const char* relative) const { return ReadText(path_ / relative); }

private:
    std::filesystem::path path_;
};

/// @return The number of line feeds in a text.
inline int CountLines(const std::string& text) {
    int count = 0;
    for (char c : text) {
        count += c == '\n' ? 1 : 0;
    }
    return count;
}

/// @return True when the text contains the part.
inline bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

/// The whole rig: a data folder, the fakes, the core. The default configuration sends to a fake sender every
/// 300 seconds (5 ticks) with the model of SPEC 8, no battery, and no session pipe message.
class Rig {
public:
    TempData data;
    FakeClock clock;
    FakePower power;
    FakeScan scan;
    CountingEmi emi;
    FakeBattery battery;
    FakeCpu cpu;
    FakeDisplay display;
    FakeSenders senders;
    ews::ServiceDeps deps;
    ews::ServiceCore core;

    /// @param name A short name of the test, part of the folder name.
    /// @param withModel False to leave calibration\model.json out.
    explicit Rig(const char* name, bool withModel = true)
        : data(name),
          deps{clock, scan, emi, battery, cpu, display, power, senders},
          core(deps, ews::ServiceSettings{data.wide(), 60}) {
        battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({});
        WriteIni("server_enabled=1\nserver_url=http://localhost:1\ninterval_seconds=300\ndevice_type=laptop\n");
        ews::StoreServerKey(data.wide(), "test-key");
        if (withModel) {
            WriteText(data.path() / "calibration" / "model.json", SpecModelText());
        }
    }

    /// Writes config\service.ini.
    void WriteIni(const std::string& text) { WriteText(data.path() / "config" / "service.ini", text); }

    /// Moves the clock by one tick and runs it, `count` times.
    void Ticks(int count) {
        for (int i = 0; i < count; ++i) {
            clock.Advance(60);
            core.Tick();
        }
    }

    /// @return The queue file text.
    std::string Queue() const { return data.Text("queue\\reports.jsonl"); }
    /// @return The status file text.
    std::string Status() const { return data.Text("status\\status.json"); }
    /// @return The service log text.
    std::string Log() const { return data.Text("logs\\service.log"); }
    /// @return The text of today's history file (the clock stays on 2026-11-14 in these tests).
    std::string History() const { return data.Text("history\\2026-11-14.csv"); }
    /// @return The line of the batch `index` of the senders, or "" when there is none.
    std::string SentLine(std::size_t batch, std::size_t line) const {
        bool there = batch < senders.batches.size();
        return there ? LineAt(senders.batches[batch], line) : std::string();
    }
};

}  // namespace ews_test
