# Research emd-1: the Intel "Energy Meter" driver (SWC\VID8086_EMD_1.0)

Research note of 11 Nov 2026. A helper returned it as text, not as a file; it is kept here word for word, with the local path masked.

Question: what is the Intel "Energy Meter" driver, which package ships it, for which processors, and does it add the Windows "Energy Meter" counters (Energy Meter Interface, EMI) with RAPL data on Windows 10 for an 8th generation Intel laptop?

Status: partial. Driver ID and versions found; shipping package, RAPL content and Windows 10 use not confirmed.

## 1. Which package ships it

- Not found on any Intel or OEM page. The only listing by name is an aggregator. Treexy: "Energy Meter", Intel, class System, ID SWC\VID8086_EMD_1.0, INF energymeterdriver.inf, versions 1.4.1003.0 (25 Jan 2026), 1.4.1002.0 (25 Jan 2026), 1.4.1001.0 (9 Jun 2025), "Windows 11, 10", x64. No description, no parent package. [aggregator] https://treexy.com/products/driver-fusion/database/system/intel/energy-meter/
- Microsoft Update Catalog, search "VID8086_EMD_1.0": four entries "Intel Corporation System Driver Update", 1.4.1003.0 and 1.4.1002.0 (three rows), class Drivers (Other Hardware), updated 1/24/2026, 521-579 KB. Products: Windows 11 22H2, 24H2 and 25H2 "and later" only; no Windows 10 row. The page shows no hardware ID, so the link to the ID rests on the search match. [Microsoft catalog] https://www.catalog.update.microsoft.com/Search.aspx?q=VID8086_EMD_1.0
- HP softpaqs contain none of "Energy Meter", "EMD", "EMI", "SWC\VID8086": IPF sp157496 (2.2.10002.4, 17 Mar 2025), IPF sp158133 (2.2.10204.8, 16 May 2025), DTT sp157490 (9.0.11702.46642), PMT sp157146 (3.1.2.6). All are older than build 1.4.1001.0, so absence proves little. [OEM pages] https://ftp.hp.com/pub/softpaq/sp158001-158500/sp158133.html
- Observation: version line 1.4.x differs from IPF 2.x, DTT 9.x, PMT 3.x, so it is probably its own driver.
- Microsoft: the system prepends SWC\ to the ComponentIDs of a software component that an extension INF creates (AddComponent) as a virtual child device under a parent device (Windows 10 1703+). Inference: Energy Meter is a child of some other Intel device. Parent not identified. [vendor doc] https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-addcomponent-directive

## 2. Platforms and Windows versions

Not found for EMD. Nearest: Intel IPF 2.3.20306.4 (14 Jul 2026) lists Meteor, Lunar, Arrow, Panther, Alder, Raptor and Wildcat Lake, Windows 11 23H2/24H2/25H2, with no Energy Meter text. [Intel vendor page] https://www.intel.com/content/www/us/en/download/826464/intel-innovation-platform-framework.html. The 2025 HP IPF/DTT pages still list Windows 10 22H2.

## 3. What it exposes

Not found. No source ties this driver to the EMI GUID, the counter set, channel names, RAPL or PSYS. Microsoft's EMI page (Windows 10 and later, GUID {45BD8344-7ED6-49cf-A440-C276C933B053}, picowatt-hours, v2 from 1809) names no hardware. [vendor doc] https://learn.microsoft.com/windows-hardware/drivers/powermeter/energy-meter-interface. Mozilla bug 1774844 (fixed, Firefox 104): Windows 11 gives CPU RAPL data; Windows 10 gives data only with hardware power meters such as Surface Book 1. It does not say which driver supplies RAPL on Windows 11. [open-source tracker] https://bugzilla.mozilla.org/show_bug.cgi?id=1774844

## 4. Whiskey Lake, Windows 10, standalone

Not found. Against: the catalog lists Windows 11 only; IPF 2.3.x is 12th gen and newer, Windows 11 only; Mozilla's Windows 10 note. For: Treexy says Windows 10 (aggregator, unverified); AddComponent exists since Windows 10 1703. No standalone Intel download named Energy Meter found. The only delivery seen is Windows Update.

## 5. Other components adding Energy Meter instances on Windows 10

None found. Intel Power Gadget's API is documented as MSR-based, no EMI mention (search snippet only) https://www.intel.com/content/www/us/en/developer/articles/training/using-the-intel-power-gadget-30-api-on-windows.html. Intel PMT driver (HP sp157146: Windows 10 22H2 and 11, Z2 G1i desktops) enumerates telemetry; no source ties it to Energy Meter. https://ftp.hp.com/pub/softpaq/sp157001-157500/sp157146.html

## Bottom line

- Energy Meter counters on Windows 10 for an 8th gen laptop through this driver: unknown, leaning no.
- The only delivery seen is Windows Update for Windows 11 22H2 and later; Intel's current framework page starts at 12th gen and Windows 11.
- The SWC\ ID means a child component of another Intel device, so the INF alone would probably create no device (inference).
- Mozilla: on Windows 10 these counters show data only with hardware meters. Nothing found shows RAPL through EMI on Windows 10.
- Tested here: the HP 250 G7 test laptop (Windows 10 22H2) has no SWC\VID8086_EMD device and no Energy Meter instance.

## Not confirmed

Shipping package and parent device; platform list; EMI/RAPL/PSYS content; channel names; Windows 10 install; any standalone Intel download.

## Search log

Queries tried for not-found items: "Intel Energy Meter driver SWC\VID8086_EMD_1.0", "energymeterdriver.inf", "Energy Meter Driver softpaq HP IPF EMD", "IPF README release notes Energy Meter", "Intel Energy Meter Lenovo Dell ASUS MSI", "Intel PMT Windows Energy Meter EMI", "RAPL_Package0_PKG Windows Energy Meter" (Linux hits only), extended search "Intel Energy Meter Driver Panther/Arrow/Lunar Lake EMI RAPL" (nothing). About 30 tool calls.

Not read: Intel IPF 2.3.20306.4 release notes PDF (the fetch tool got compressed bytes it could not read); Intel PPM PPP and SD IPF PPP release notes (HTTP 403); Intel download pages 761726 and 842294 (redirect to corpredirect.intel.com not followed); catalog details pages and the INF itself (no downloads allowed); unofficial mirror sites (not opened); FOSDEM 2024 slides (search snippet only); Lenovo, Dell, ASUS, MSI (one combined query, no Energy Meter page appeared).
