// test_emi_kind.cpp: tests of the new EMI channel kind cpu_core (SPEC 3.3): AMD laptops list one channel per core.
#include <string>

#include "ews/emi_kind.h"
#include "test_common.h"

using ews::EmiKind;

EWS_TEST(KindCpuCoreFromAmdChannelNames) {
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_Core0_CORE") == EmiKind::CpuCore);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_Core2_CORE") == EmiKind::CpuCore);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_Core14_CORE") == EmiKind::CpuCore);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package1_Core3_CORE") == EmiKind::CpuCore);
}

EWS_TEST(KindCpuCoreIgnoresCase) {
    CHECK(ews::ClassifyEmiChannel("rapl_package0_core0_core") == EmiKind::CpuCore);
    CHECK(ews::ClassifyEmiChannel("RAPL_PACKAGE0_CORE5_CORE") == EmiKind::CpuCore);
}

EWS_TEST(KindCpuCoreNeedsTheWholeShape) {
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_Core_CORE") == EmiKind::Other);    // no core number
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_Core0") == EmiKind::Other);        // no tail
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_Core0_COREX") == EmiKind::Other);  // longer tail
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_Corex_CORE") == EmiKind::Other);   // not a number
    CHECK(ews::ClassifyEmiChannel("RAPL_Package_Core0_CORE") == EmiKind::Other);    // no package number
    CHECK(ews::ClassifyEmiChannel("Core0_CORE") == EmiKind::Other);
}

EWS_TEST(KindOtherRulesStayAsTheyWere) {
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_PKG") == EmiKind::CpuPackage);
    CHECK(ews::ClassifyEmiChannel("RAPL_Package0_PP0") == EmiKind::CpuCores);
    CHECK(ews::ClassifyEmiChannel("CPU_BIG") == EmiKind::CpuCores);
    CHECK(ews::ClassifyEmiChannel("VDDCR_VDD Energy") == EmiKind::Other);
    CHECK(ews::ClassifyEmiChannel("VDDCR_SOC Energy") == EmiKind::Other);
    CHECK(ews::ClassifyEmiChannel("Current Socket Energy") == EmiKind::Other);
    CHECK(ews::ClassifyEmiChannel("Apu Energy") == EmiKind::Other);
}

EWS_TEST(KindCpuCoreWord) { CHECK(std::string(ews::EmiKindWord(EmiKind::CpuCore)) == "cpu_core"); }
