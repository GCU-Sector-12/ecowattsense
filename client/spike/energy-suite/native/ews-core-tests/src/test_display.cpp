// test_display.cpp: tests of the pure display helper BrightnessPercent.
#include "ews/display.h"
#include "test_common.h"

using ews::DisplayBrightness;
using ews::Status;

EWS_TEST(BrightnessPicksMainsOrBattery) {
    DisplayBrightness levels{80, 40};
    auto mains = ews::BrightnessPercent(levels, false);
    CHECK(mains.status == Status::Ok);
    CHECK(mains.value == 80);
    auto battery = ews::BrightnessPercent(levels, true);
    CHECK(battery.status == Status::Ok);
    CHECK(battery.value == 40);
}

EWS_TEST(BrightnessHundredIsOkAndAboveIsError) {
    CHECK(ews::BrightnessPercent(DisplayBrightness{100, 100}, false).status == Status::Ok);
    CHECK(ews::BrightnessPercent(DisplayBrightness{100, 100}, false).value == 100);
    CHECK(ews::BrightnessPercent(DisplayBrightness{101, 50}, false).status == Status::Error);
    CHECK(ews::BrightnessPercent(DisplayBrightness{50, 101}, true).status == Status::Error);
}

EWS_TEST(BrightnessIgnoresTheLevelThatDoesNotApply) {
    // A bad value on the unused side must not spoil the answer.
    CHECK(ews::BrightnessPercent(DisplayBrightness{50, 255}, false).status == Status::Ok);
    CHECK(ews::BrightnessPercent(DisplayBrightness{255, 50}, true).status == Status::Ok);
}

EWS_TEST(BrightnessZeroIsOk) {
    auto r = ews::BrightnessPercent(DisplayBrightness{0, 0}, true);
    CHECK(r.status == Status::Ok);
    CHECK(r.value == 0);
}
