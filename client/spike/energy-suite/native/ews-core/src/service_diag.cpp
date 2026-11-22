// service_diag.cpp: the diagnostics file part of ServiceCore (see ews/service_core.h and SPEC 7a).
// Used by ews-service and the tests. It only records: it fills the plain structs of ews/diag_line.h from values the
// service already has at the tick, and hands the finished line to the recorder. It adds no read of any device and no
// timer, and it never changes what the service measures, reports or sends. A value that is not known is left out.
#include <algorithm>
#include <cmath>

#include "ews/service_core.h"

namespace ews {

namespace {

/// @return True when a counter that was known before is lower now.
bool WentBack(const EmiSample& before, const EmiSample& now) {
    return before.known && now.known && now.energyPicowattHours < before.energyPicowattHours;
}

/// @return The growth of one counter in whole uJ, or nothing when a sample is unknown or the counter went back.
std::optional<std::uint64_t> GrowthUj(const EmiSample& before, const EmiSample& now) {
    bool usable = before.known && now.known && now.energyPicowattHours >= before.energyPicowattHours;
    if (!usable) {
        return std::nullopt;
    }
    Reading<std::uint64_t> micro = PicowattHoursToMicrojoules(now.energyPicowattHours - before.energyPicowattHours);
    return micro.ok() ? std::optional<std::uint64_t>(micro.value) : std::nullopt;
}

/// @param channel The channel.
/// @param now The sample of this tick, or null when the whole read failed.
/// @param before The sample of the earlier read, or null when it is not usable.
/// @return The channel for the line: `ok` false without raw and energy when the read failed.
DiagEmi DiagChannel(const EmiChannel& channel, const EmiSample* now, const EmiSample* before) {
    DiagEmi out;
    out.name = channel.name;
    out.kind = EmiKindWord(channel.kind);
    out.ok = now != nullptr && now->known;
    if (!out.ok) {
        return out;
    }
    out.raw = now->energyPicowattHours;
    out.uj = before == nullptr ? std::nullopt : GrowthUj(*before, *now);
    return out;
}

/// @return One entry for each channel, in the order of the channel list.
std::vector<DiagEmi> DiagEmiList(const std::vector<EmiChannel>& channels, const Snapshot& previous, const Snapshot& current) {
    bool readOk = current.emiKnown && current.emi.size() == channels.size();
    bool pairOk = readOk && previous.emiKnown && previous.emi.size() == channels.size();
    std::vector<DiagEmi> list;
    for (std::size_t index = 0; index < channels.size(); ++index) {
        const EmiSample* now = readOk ? &current.emi[index] : nullptr;
        const EmiSample* before = pairOk ? &previous.emi[index] : nullptr;
        list.push_back(DiagChannel(channels[index], now, before));
    }
    return list;
}

/// @return True when a counter of a channel went backwards between the two reads.
bool AnyCounterBack(std::size_t channelCount, const Snapshot& previous, const Snapshot& current) {
    bool comparable = previous.emi.size() == channelCount && current.emi.size() == channelCount;
    if (!comparable) {
        return false;
    }
    bool back = false;
    for (std::size_t index = 0; index < channelCount; ++index) {
        back = back || WentBack(previous.emi[index], current.emi[index]);
    }
    return back;
}

/// @return One battery as read. A capacity or rate the driver marks unknown is left out.
DiagBattery DiagBatteryOf(const BatteryState& state) {
    DiagBattery out;
    out.ok = true;
    out.unitMwh = state.unit == BatteryUnit::MilliwattHours;
    out.remaining = state.remainingKnown ? std::optional<std::uint64_t>(state.remaining) : std::nullopt;
    out.full = state.fullChargedKnown ? std::optional<std::uint64_t>(state.fullCharged) : std::nullopt;
    out.design = state.designedKnown ? std::optional<std::uint64_t>(state.designed) : std::nullopt;
    out.rate = state.rateKnown ? std::optional<std::int64_t>(state.rate) : std::nullopt;
    out.voltage = state.voltageKnown ? std::optional<std::uint64_t>(state.voltage) : std::nullopt;
    out.online = state.onLine;
    out.charging = state.charging;
    out.discharging = state.discharging;
    return out;
}

/// @return Every battery of the snapshot. A failed read gives one entry with `ok` false; no battery gives an empty list.
std::vector<DiagBattery> DiagBatteries(const Snapshot& snapshot) {
    if (!snapshot.batteryReadOk) {
        return std::vector<DiagBattery>(1);
    }
    std::vector<DiagBattery> list;
    for (const BatteryState& state : snapshot.batteries) {
        list.push_back(DiagBatteryOf(state));
    }
    return list;
}

/// Adds the word to the list when the condition holds.
void AddFlag(std::vector<std::string>& flags, bool applies, const char* word) {
    if (applies) {
        flags.push_back(word);
    }
}

/// @return The energy of one minute for the report sum (SPEC 4.1 and 4.2 applied to one minute): 0 on battery;
///         on mains the device energy plus the positive part of the battery change. Nothing when a value that is
///         needed for it is unknown.
std::optional<std::int64_t> MinuteReportEnergyUj(bool onBattery, bool batteryPresent, const MinuteEnergy& energy, const MinuteBattery& battery) {
    if (onBattery) {
        return 0;
    }
    if (!energy.deviceUj.has_value()) {
        return std::nullopt;
    }
    if (!batteryPresent) {
        return static_cast<std::int64_t>(std::llround(*energy.deviceUj));
    }
    if (!battery.changeUj.has_value()) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(std::llround(*energy.deviceUj + std::max(0.0, *battery.changeUj)));
}

/// @return The display facts of the tick.
DiagDisplay DiagDisplayOf(bool on, const std::optional<double>& brightness, const char* from) {
    DiagDisplay display;
    display.on = on;
    // An unknown brightness counts as 50 in the calculation (SPEC 4.2); the line says so with `default`.
    display.brightnessPct = static_cast<std::int64_t>(std::llround(brightness.value_or(50.0)));
    display.from = from;
    return display;
}

}  // namespace

void ServiceCore::DiagWrite(UtcTime time, const std::string& line) {
    DiagOutcome outcome = diag_.Record(time, line);
    if (outcome.note == DiagNote::Full) {
        Log(LogEvent::DiagFull);
    }
    if (outcome.note == DiagNote::WriteFailed) {
        Log(LogEvent::DiagWriteFailed, {outcome.failures});
    }
}

void ServiceCore::DiagEvent(const std::string& what, const std::vector<DiagField>& fields) {
    if (!diag_.Enabled()) {
        return;
    }
    UtcTime now = deps_.clock.Now();
    DiagWrite(now, BuildEventLine(ToUnixSeconds(now), what, fields));
}

DiagSession ServiceCore::DiagSessionAt(std::int64_t nowUnix) const {
    DiagSession session;
    session.loggedOn = somebodyLoggedOn_;
    session.locked = activeSessionLocked_;
    std::int64_t idle = ComputeIdleSeconds(activity_, nowUnix, serviceStartUnix_);
    session.idleS = idle;
    session.idleMsgAgeS = activity_.hasMessage ? std::optional<std::int64_t>(std::max<std::int64_t>(0, nowUnix - activity_.receivedUnix)) : std::nullopt;
    session.state = ComputeState(idle, config_.idleThresholdSeconds, config_.unusedThresholdSeconds, !somebodyLoggedOn_);
    return session;
}

DiagCalc ServiceCore::DiagCalcOf(const Snapshot& current, const DiagMinute& minute) const {
    DiagCalc calc;
    calc.scanLevel = LevelWord(scanLevel_);
    calc.level = MeasureLevelWord(minute.shown.level);
    calc.model = model_.has_value();
    bool measured = minute.shown.level != MeasureLevel::Unknown;
    calc.measuredUj = measured ? std::optional<std::int64_t>(std::llround(minute.shown.measuredUj)) : std::nullopt;
    calc.baseW = minute.energy.terms.baseW;
    calc.restW = minute.energy.terms.restW;
    calc.cpuTermW = minute.energy.terms.cpuW;
    calc.screenW = minute.energy.terms.screenW;
    calc.deviceW = EnergyToWatts(minute.energy.deviceUj, minute.seconds);
    calc.batteryW = EnergyToWatts(minute.battery.changeUj, minute.seconds);
    calc.energyUj = MinuteReportEnergyUj(onBattery_, current.battery.present, minute.energy, minute.battery);
    return calc;
}

std::vector<std::string> ServiceCore::DiagFlagsOf(const Snapshot& current, const DiagMinute& minute) const {
    std::vector<std::string> flags;
    AddFlag(flags, minute.battery.jump, "battery_jump");
    AddFlag(flags, minuteFlags_.batteryReplaced, "battery_replaced");
    AddFlag(flags, minuteFlags_.held, "held");
    AddFlag(flags, previousRead_ == PreviousRead::AfterGap, "gap");
    AddFlag(flags, AnyCounterBack(channels_.size(), previous_, current), "counter_back");
    AddFlag(flags, previousRead_ == PreviousRead::First, "first_read");
    return flags;
}

void ServiceCore::DiagTickLine(const Snapshot& current, const DiagMinute& minute) {
    if (!diag_.Enabled()) {
        return;
    }
    std::int64_t nowUnix = ToUnixSeconds(current.time);
    DiagTick tick;
    tick.unixSeconds = nowUnix;
    tick.seconds = static_cast<std::int64_t>(std::llround(minute.seconds));
    tick.onBattery = onBattery_;
    tick.emi = DiagEmiList(channels_, previous_, current);
    tick.battery = DiagBatteries(current);
    tick.cpuPct = minute.cpuPercent;
    tick.display = DiagDisplayOf(displayOn_, minute.brightness.percent, minute.brightness.from);
    tick.session = DiagSessionAt(nowUnix);
    tick.calc = DiagCalcOf(current, minute);
    tick.flags = DiagFlagsOf(current, minute);
    DiagWrite(current.time, BuildTickLine(tick));
}

void ServiceCore::DiagReportLine(bool queued, const std::string& reportJson) {
    if (!queued || !diag_.Enabled()) {
        return;
    }
    UtcTime now = deps_.clock.Now();
    DiagWrite(now, BuildReportLine(ToUnixSeconds(now), UnsentCount(), reportJson));
}

void ServiceCore::DiagSendLine(const SendResult& result, int lines) {
    if (!diag_.Enabled()) {
        return;
    }
    bool answered = result.httpStatus > 0;
    bool taken = result.word == SendWord::Ok;
    DiagSend send;
    send.trigger = sendTrigger_;
    send.reports = lines;
    send.result = SendResultWord(lastSend_);
    send.http = answered ? std::optional<int>(result.httpStatus) : std::nullopt;
    send.rejected = answered ? std::optional<int>(result.rejected) : std::nullopt;
    send.created = answered && taken ? std::optional<int>(result.created) : std::nullopt;
    send.duplicate = answered && taken ? std::optional<int>(result.duplicate) : std::nullopt;
    send.queue = UnsentCount();
    UtcTime now = deps_.clock.Now();
    DiagWrite(now, BuildSendLine(ToUnixSeconds(now), send));
}

void ServiceCore::DiagPingChanged(LastPing before) {
    if (lastPing_ == before) {
        return;
    }
    DiagEvent("ping", {FieldWord("result", PingResultWord(lastPing_))});
}

}  // namespace ews
