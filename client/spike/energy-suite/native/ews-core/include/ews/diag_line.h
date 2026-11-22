// diag_line.h: builds the text of the lines of the diagnostics file (SPEC 7a): tick, report, send and event.
// Used by ServiceCore (service_diag.cpp), which fills the plain structs below, and by the tests. No Windows call is made
// here and no enum of the core is used, so the file also builds and runs on any compiler.
// A value that is not known stays empty and is left out of the line. It is never written as 0.
// Only numbers, fixed words and the names of the EMI channels can reach a line (plus the report object itself).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ews {

/// The longest line in bytes, with its line feed (SPEC 7a). A longer line is not written.
inline constexpr std::size_t kMaxDiagLineBytes = 16 * 1024;

/// One EMI channel at one tick.
struct DiagEmi {
    std::string name;                    ///< The channel name as the driver gave it (written escaped).
    std::string kind;                    ///< The fixed word of the kind, for example `cpu_package`.
    bool ok = false;                     ///< The read of this channel worked.
    std::optional<std::uint64_t> raw;    ///< The counter exactly as read (pWh); empty when the read failed.
    std::optional<std::uint64_t> uj;     ///< The energy of this minute in uJ; empty when it is not known.
};

/// One battery at one tick, as read.
struct DiagBattery {
    bool ok = false;                          ///< The read worked. When false only `ok` is written.
    bool unitMwh = true;                      ///< The capacities are mWh. A relative unit leaves the mWh and mW fields out.
    std::optional<std::uint64_t> remaining;   ///< remaining_mwh
    std::optional<std::uint64_t> full;        ///< full_mwh
    std::optional<std::uint64_t> design;      ///< design_mwh
    std::optional<std::int64_t> rate;         ///< rate_mw, negative while discharging
    std::optional<std::uint64_t> voltage;     ///< voltage_mv
    bool online = false;
    bool charging = false;
    bool discharging = false;
};

/// The display at one tick.
struct DiagDisplay {
    bool on = true;
    std::optional<std::int64_t> brightnessPct;
    std::string from;                         ///< `service`, `pipe` or `default`.
};

/// The session facts at one tick.
struct DiagSession {
    bool loggedOn = false;
    bool locked = false;
    std::optional<std::int64_t> idleS;
    std::optional<std::int64_t> idleMsgAgeS;  ///< Seconds since the last accepted pipe message; empty when there was none.
    std::optional<std::int64_t> state;        ///< 2, 1 or 0.
};

/// The numbers the minute was calculated with.
struct DiagCalc {
    std::string scanLevel;                    ///< `L1` to `L4`.
    std::string level;                        ///< The level used for this minute; empty when the minute is unknown.
    bool model = false;                       ///< A valid model is loaded.
    std::optional<std::int64_t> measuredUj;
    std::optional<double> baseW;              ///< Only at L3.
    std::optional<double> restW;              ///< Only at L2.
    std::optional<double> cpuTermW;
    std::optional<double> screenW;
    std::optional<double> deviceW;
    std::optional<double> batteryW;
    std::optional<std::int64_t> energyUj;     ///< The energy of the minute for the report sum.
};

/// Everything of one tick line.
struct DiagTick {
    std::int64_t unixSeconds = 0;
    std::optional<std::int64_t> seconds;
    bool onBattery = false;
    std::vector<DiagEmi> emi;
    std::vector<DiagBattery> battery;
    std::optional<double> cpuPct;
    DiagDisplay display;
    DiagSession session;
    DiagCalc calc;
    std::vector<std::string> flags;           ///< Fixed words: battery_jump, battery_replaced, held, gap, counter_back, first_read.
};

/// One send try.
struct DiagSend {
    std::string trigger;                      ///< `tick`, `network`, `pipe` or `command`.
    int reports = 0;
    std::string result;                       ///< The word of last_send_result.
    std::optional<int> http;                  ///< The HTTP status; empty when there was no answer.
    std::optional<int> created;               ///< Empty when the answer did not say.
    std::optional<int> duplicate;
    std::optional<int> rejected;
    std::optional<int> queue;                 ///< Reports left in the queue; empty when it could not be counted.
};

/// One extra field of an event line. An empty `json` means the value is unknown and the field is left out.
struct DiagField {
    std::string key;
    std::string json;                         ///< A ready JSON value.
};

/// @param key The field name. @param value The value. @return A bool field.
DiagField FieldBool(const std::string& key, bool value);
/// @param key The field name. @param value The value. @return A whole number field.
DiagField FieldInt(const std::string& key, long long value);
/// @param key The field name. @param value The value or nothing. @return A whole number field, or one that is left out.
DiagField FieldMaybeInt(const std::string& key, const std::optional<long long>& value);
/// @param key The field name. @param word A fixed word or a checked text. @return A text field (escaped); an empty word is unknown and is left out.
DiagField FieldWord(const std::string& key, const std::string& word);

/// @param tick The values of one tick.
/// @return The line `{"t":"tick","ts":"...",...}` without a line feed.
std::string BuildTickLine(const DiagTick& tick);

/// @param unixSeconds The time of the line.
/// @param queue Reports in the queue after this one was added.
/// @param reportJson The report exactly as queued (one line of JSON).
/// @return The line `{"t":"report",...}` without a line feed, or an empty text when `reportJson` is empty or has a line break.
std::string BuildReportLine(std::int64_t unixSeconds, int queue, const std::string& reportJson);

/// @param unixSeconds The time of the line.
/// @param send The send try.
/// @return The line `{"t":"send",...}` without a line feed.
std::string BuildSendLine(std::int64_t unixSeconds, const DiagSend& send);

/// @param unixSeconds The time of the line.
/// @param what The fixed word of the event (`start`, `power_source`, ...).
/// @param fields The extra fields, in order; a field with an empty value is left out.
/// @return The line `{"t":"event","ts":"...","what":"...",...}` without a line feed.
std::string BuildEventLine(std::int64_t unixSeconds, const std::string& what, const std::vector<DiagField>& fields);

/// @param line A line without its line feed.
/// @return True when the line and its line feed are at most kMaxDiagLineBytes.
bool FitsDiagLine(const std::string& line);

}  // namespace ews
