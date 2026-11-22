// test_display_layout.cpp: tests of the display layout rules (SPEC 1.4): counts, built-in panel, refresh rate.
#include <vector>

#include "ews/display_layout.h"
#include "test_common.h"

using ews::DisplayPathFacts;

namespace {

/// Builds the facts of one path.
DisplayPathFacts Path(bool internalPanel, std::uint32_t numerator, std::uint32_t denominator) {
    DisplayPathFacts path;
    path.internalPanel = internalPanel;
    path.refreshNumerator = numerator;
    path.refreshDenominator = denominator;
    return path;
}

}  // namespace

EWS_TEST(DisplayInternalTechnologies) {
    CHECK(ews::IsInternalOutputTechnology(0x80000000u));  // INTERNAL
    CHECK(ews::IsInternalOutputTechnology(11));           // DISPLAYPORT_EMBEDDED
    CHECK(ews::IsInternalOutputTechnology(13));           // UDI_EMBEDDED
    CHECK(ews::IsInternalOutputTechnology(6));            // LVDS
    CHECK(!ews::IsInternalOutputTechnology(5));           // HDMI
    CHECK(!ews::IsInternalOutputTechnology(10));          // DISPLAYPORT_EXTERNAL
    CHECK(!ews::IsInternalOutputTechnology(0xFFFFFFFFu)); // OTHER (-1)
}

EWS_TEST(DisplayRefreshHertzRoundsToNearest) {
    CHECK(ews::RefreshHertz(60, 1).value_or(0) == 60);
    CHECK(ews::RefreshHertz(60000, 1001).value_or(0) == 60);   // 59.94
    CHECK(ews::RefreshHertz(144000, 1000).value_or(0) == 144);
    CHECK(ews::RefreshHertz(119880, 1000).value_or(0) == 120); // 119.88
}

EWS_TEST(DisplayRefreshHertzRejectsBadRates) {
    CHECK(!ews::RefreshHertz(60, 0).has_value());
    CHECK(!ews::RefreshHertz(0, 1).has_value());
    CHECK(!ews::RefreshHertz(1, 1000).has_value());      // 0.001 Hz
    CHECK(!ews::RefreshHertz(5000, 1).has_value());      // above 1000 Hz
}

EWS_TEST(DisplayLaptopWithOneExternalMonitor) {
    std::vector<DisplayPathFacts> paths{Path(true, 60, 1), Path(false, 144, 1)};
    auto layout = ews::SummariseDisplayPaths(paths);
    CHECK(layout.displays.value_or(99) == 2);
    CHECK(layout.externalDisplays.value_or(99) == 1);
    CHECK(layout.refreshHz.value_or(0) == 60);
}

EWS_TEST(DisplayRefreshComesFromTheBuiltInPanelEvenWhenItIsSecond) {
    std::vector<DisplayPathFacts> paths{Path(false, 144, 1), Path(true, 60, 1)};
    CHECK(ews::SummariseDisplayPaths(paths).refreshHz.value_or(0) == 60);
}

EWS_TEST(DisplayRefreshOfTheFirstDisplayWithoutABuiltInPanel) {
    std::vector<DisplayPathFacts> paths{Path(false, 75, 1), Path(false, 60, 1)};
    auto layout = ews::SummariseDisplayPaths(paths);
    CHECK(layout.displays.value_or(99) == 2);
    CHECK(layout.externalDisplays.value_or(99) == 2);
    CHECK(layout.refreshHz.value_or(0) == 75);
}

EWS_TEST(DisplayBuiltInPanelWithoutARateLeavesRefreshOut) {
    std::vector<DisplayPathFacts> paths{Path(true, 0, 0), Path(false, 60, 1)};
    auto layout = ews::SummariseDisplayPaths(paths);
    CHECK(layout.displays.value_or(99) == 2);
    CHECK(!layout.refreshHz.has_value());
}

EWS_TEST(DisplayNoPathsIsZeroDisplaysAndNoRate) {
    auto layout = ews::SummariseDisplayPaths(std::vector<DisplayPathFacts>{});
    CHECK(layout.displays.value_or(99) == 0);
    CHECK(layout.externalDisplays.value_or(99) == 0);
    CHECK(!layout.refreshHz.has_value());
}

EWS_TEST(DisplayAnUnbelievableCountIsUnknown) {
    std::vector<DisplayPathFacts> paths(300, Path(false, 60, 1));
    auto layout = ews::SummariseDisplayPaths(paths);
    CHECK(!layout.displays.has_value());
    CHECK(!layout.externalDisplays.has_value());
    CHECK(!layout.refreshHz.has_value());
}
