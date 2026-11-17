# Vendor-signed and in-box ways (other than the Windows 11 Energy Meter Interface) to read real power or energy on Windows 10/11

Research date: 11 Nov 2026. Scope: Intel PMT, Intel IPF/DTT, AMD (PMF, Ryzen Master Monitoring SDK, uProf), Dell/HP/Lenovo WMI, ACPI 4.0 power meters, USB-C PD, PawnIO. Already known and not redone: Intel Power Gadget discontinued, WinRing0 flagged by Defender, Intel PCM msr.sys not Microsoft-signed, DTT WMI EsifDeviceInformation (admin only, tenths of a watt).

How to read the tags at the end of each finding:
- (direct) = I fetched and read the page.
- (PDF read locally) = the fetch tool saved the PDF; I extracted its text with pdftotext and read it.
- (search summary only) = I only saw it in a search-engine result summary; the page itself failed to fetch (403 or timeout) or was not fetched. Treat as lower confidence.
- Dates: "2026" = current; "older: year" = historical.
- Wording is paraphrased on purpose; code names and commands are literal.

## 1. Intel Platform Monitoring Technology (PMT) on Windows: does its driver expose energy or power telemetry to user mode, and for which generations?

### Takeaway
A real Windows PMT driver exists (OEM-shipped "Intel Platform Monitoring Technology (PMT) Driver", IntcPMT) and Intel's own software (SoC Watch, the IPF PMT extension provider) reads telemetry through it, starting with Tiger Lake. I found no public Windows user-mode API, WMI class or performance counter for it, so for an independent program it is an Intel-tools-only path in 2026.

### Cited Findings
- Intel describes PMT as a standard way to expose telemetry in-band and out-of-band, built from discovery, aggregator, watcher, schema and access components. The Intel-PMT repository documents only the Linux mainline driver: sysfs files under /sys/class/intel_pmt/telem* (guid, size, telem), decoded with XML definitions selected by GUID. Licence Apache-2.0. No Windows support or Windows API is mentioned. (direct) — [Intel-PMT on GitHub](https://github.com/intel/Intel-PMT)
- Intel's PMT technical specification says the telemetry gives a more detailed view of system operation and in some cases is used to diagnose power consumption issues. (search summary only) — [Intel PMT technical specification](https://www.intel.com/content/www/us/en/content-details/710389/intel-platform-monitoring-technology-intel-pmt-technical-specification.html)
- Energy counters are named only in a Linux, server-oriented example: the Telegraf intel_pmt plugin lists metrics such as DRAM energy in microjoules. Nothing there is specific to Windows client CPUs. (search summary only) — [Telegraf intel_pmt plugin](https://docs.influxdata.com/telegraf/v1/input-plugins/intel_pmt/)
- A Windows driver exists. HP SoftPaq sp151256: "Intel Platform Monitoring Technology (PMT) Driver" 3.1.2.2 Rev A, dated 31 Jan 2024, for Windows 11 23H2 on the HP Envy 17 notebook, device ID PCI\VEN_8086&DEV_7D0D. Described only as the driver that enables PMT. (direct; older: 2024) — [HP sp151256](https://ftp.ext.hp.com/pub/softpaq/sp151001-151500/sp151256.html)
- HP also lists a PMT driver titled "(Core Series 3)", so OEMs still ship it for current platforms. Only the title was seen. (search summary only) — [HP sp173087](https://ftp.ext.hp.com/pub/softpaq/sp173001-173500/sp173087.html)
- Intel SoC Watch for Windows v2022.5 (Sept 2022): the PMT driver must be loaded to collect PMT-based metrics on Intel client platforms from Tiger Lake onward. SoC Watch runs on Windows 10 and 11 only with administrator permissions and is delivered inside the oneAPI Base Toolkit / VTune Profiler (also usable standalone). (PDF read locally; older: 2022) — [SoC Watch for Windows release notes v2022.5](https://www.intel.com/content/dam/develop/external/us/en/documents/release_notes_socwatch_windows_v2022.5.pdf)
- SoC Watch notes list the Tiger Lake PMT-sourced metrics as display, IPU, IO, LLC, PCIe and Type-C bandwidth, plus discrete-graphics power on Iris Xe. These are not CPU package or platform energy. (search summary only; older) — [Intel SoC Watch release notes](https://www.intel.com/content/www/us/en/developer/articles/release-notes/intel-soc-watch-release-notes.html)
- The IPF "System Data" package contains a PMT extension provider that needs the IntcPMT driver; the installer skips the component if the driver is absent. Prerequisites in release notes 03.04.1005: administrator privileges, Visual C++ Redistributable 2019 or later, IPF 2.3.20099.3619 or later. (search summary only; direct fetch returned HTTP 403) — [IPF System Data release notes 03.04.1005](https://downloadmirror.intel.com/869522/lntel_SD_IPF_PPP_Release_Notes_03.04.1005.pdf)
- A newer System Data release lists Windows 11 24H2 and 25H2 on Core Ultra 200S Plus and Core Ultra Series 3, with the same IntcPMT requirement for the PMT provider. (search summary only; 2026) — [IPF System Data release notes (latest)](https://downloadmirror.intel.com/923214/lntel_SD_IPF_PPP_Release_Notes.pdf)

### Inferences
- Generations: the Windows PMT driver and SoC Watch support start at Tiger Lake; OEM packages exist for 2024 and 2026 platforms. The Windows driver is not tied to server parts.
- The only consumers I could document are Intel software (SoC Watch with admin, IPF providers installed with admin). A third-party program would need an undocumented device interface or the restricted IPF layer (section 2), so PMT is not a supported route for an independent application.
- Whether client PMT telemetry contains package or platform energy counters cannot be concluded from what I read; the documented Tiger Lake examples are bandwidth and discrete-GPU power.

### Gaps
- No documentation found of the IntcPMT device interface (GUID, IOCTLs), its access control (standard user vs administrator), or any WMI class or perf counter built on it.
- The IPF System Data release-note PDFs returned HTTP 403 for direct fetch, so the PMT provider details are search-summary only.
- Which client generations expose energy or power counters (Meteor Lake, Lunar Lake, Arrow Lake, Panther Lake) was not found; the client XML definitions in the Intel-PMT repository were not read.
- Redistribution terms of the IntcPMT driver were not found.
- A search for HWiNFO using PMT returned nothing specific.

## 2. Intel Innovation Platform Framework (IPF) and Dynamic Tuning: is there a documented SDK or API for apps (ipf_sdk, IPF client), with which rights; esif_uf shell and web interface: rights and availability

### Takeaway
IPF is a Windows 11-only abstraction layer that Intel says gives only select Intel software access to platform data; I found no public IPF SDK, client library or API reference (nothing called ipf_sdk). The only app-facing offer is "IPF Providers for Device Management" with a namespace-based API for management software, shipped as an installer under Intel's licence with no developer documentation. The open-source DPTF/ESIF repository (source of esif_uf) was archived on 31 Aug 2026 with no support.

### Cited Findings
- IPF download page: v2.3.20306.4 is marked Latest, dated 7/14/2026; Windows 11 23H2, 24H2, 25H2; Core Ultra (Meteor Lake, Lunar Lake, Arrow Lake, Panther Lake) and Core (Alder Lake, Raptor Lake, Wildcat Lake); only an installer (Ipf_2.3.20306.4_Install.exe, 30.4 MB); documents offered are release notes plus signature and SBOM files, no SDK or API; licence is the Intel Software License Agreement, acceptance required. IPF is described as letting select Intel software access platform information and is required by the latest XTU 10 on unlocked Core Ultra (Series 2) or newer. (direct; 2026) — [Intel IPF download page](https://intel.com/content/www/us/en/download/826464/intel-innovation-platform-framework.html)
- IPF release notes dated 13 July 2026 (PDF read locally): abstraction layer that gives select applications access to and control of platform capabilities; supported platforms Core Ultra 200S Plus, Series 3, Series 2; fixes include a race-condition fix for the UDS pipes used by DTT, CST and XTU, and a fix for high CPU use caused by IPF (the document does not expand UDS or CST). Intel's note says Windows Update or the OEM site is the recommended source and a generic install from the Platform Performance Package is at the user's own responsibility. — [IPF PPP release notes 2.3.20306.4](https://downloadmirror.intel.com/927220/Intel_IPF_PPP_Release_Notes_2.3.20306.4.pdf)
- IPF Windows release notes dated 14 July 2026 (PDF read locally): install IPF only if Intel XTU asks for it; owners of 14th Gen Core or older should not download it; for XTU, IPF is needed only on Arrow Lake and newer; installation needs acceptance of the licence agreement. — [IPF Win release notes 2.3.20306.4](https://downloadmirror.intel.com/923740/IPF_Win_Release_Notes_2.3.20306.4.pdf)
- "IPF Providers for Device Management" 21.0.0005.0, dated 3/25/2026: lets applications access platform data, configure properties and run tasks in the manageability, security and telemetry domains through a namespace-based API architecture; Windows 11 24H2 and 25H2; Meteor Lake, Lunar Lake, Arrow Lake-S/H, Panther Lake, Alder Lake, Raptor Lake; Intel Software License Agreement; the page offers only the 4.4 MB installer and a release-notes PDF, no SDK or API reference. (direct; 2026) — [IPF Providers for Device Management](https://www.intel.com/content/www/us/en/download/916077/intel-ipf-providers-for-device-management.html)
- Intel Platform Performance Package (IPPP) article: one installer that bundles 7 drivers and 1 application (IPF, DTT, PPM, Application Optimization, several IPF providers) for Core Ultra 200S Plus/200HX Plus and Core Ultra Series 3 on Windows 11 25H2; IPF is again described as giving select Intel software access to platform information; no SDK or API is named; reboot required. (direct; 2026) — [Intel Platform Performance Package article](http://www.intel.com/content/www/us/en/support/articles/000102685/processors.html)
- The System Data provider package lists Visual Studio 2022 (toolset v143) and WiX 5.0.2 or later as developer prerequisites, which suggests provider development exists but no public SDK pointer was found. (search summary only) — [IPF System Data release notes](https://downloadmirror.intel.com/923214/lntel_SD_IPF_PPP_Release_Notes.pdf)
- Dynamic Tuning Technology release notes (PDF read locally): driver 9.1.10011.2963 dated 28 July 2026; Core Ultra 200S Plus, 200HX Plus, Series 3; Windows 11 23H2/24H2/25H2; requires IPF 2.2.10204 or higher; OEM-tested drivers recommended. — [DTT release notes 9.1.10011.2963](https://downloadmirror.intel.com/927220/Intel_DTT_PPP_Release_Notes_9.1.10011.2963.pdf)
- Intel support guidance: DTT drivers come from the system maker because they are OEM-customised; end users cannot manually install or enable DTT unless the manufacturer supports and configures it. (search summary only) — [Intel DTT support article](https://www.intel.com/content/www/us/en/support/articles/000102775.html)
- Older DTT builds exist for Windows 10 through OEM and third-party driver sites, for example ASUS ProArt B760 DTT 9.0.11405.42569 for Windows 10 64-bit. (search summary only; third-party listing) — [Softpedia listing](https://drivers.softpedia.com/get/MOTHERBOARD/Asus/ASUS-ProArt-B760-CREATOR-D4-Intel-DTT-Driver-9-0-11405-42569-for-Windows-10-64-bit.shtml)
- intel/dptf on GitHub is the Intel Dynamic Tuning for Chromium OS 9.x source release; archived by Intel on 31 Aug 2026 (read-only); Intel states it will not provide or guarantee development or support and no longer accepts patches; the Linux build produces the ipf_ufd daemon and needs root and BIOS DPTF ACPI objects; the page does not document the esif_uf shell or any power-reading command; licence text not visible. (direct; 2026) — [intel/dptf](https://github.com/intel/dptf)
- Unofficial documentation of the ESIF shell (not from Intel): on Windows the shell is started as `esif_uf.exe client` from the DriverStore folder of the dptf_cpu.inf package; headless form `esif_uf.exe client -c "<commands>" -q`; `esif_uf.exe reload` restarts DPTF; commands include `parts`, `infofpc <participant>`, `dstn <participant>`, `getp <id>` (for example `getp 128 D0 255` labelled an energy counter, `getp 291` PMAX in mW), `participantlog start TCPU all all`, and `web start` / `web stop`; no port or URL is given for the web interface, `ipfclient` is mentioned without detail, and rights are not stated. (direct; unofficial) — [ESIF Commands (Atiq's Doc)](https://mohammadarahman.github.io/docs/dptf/esif.html)

### Inferences
- As of Nov 2026 no public IPF SDK or client API for independent applications exists; the layer is built for Intel's own tools (XTU, DTT and similar) and for management software that uses the licensed Device Management providers. An independent energy app cannot count on IPF.
- On Windows 10 the 2026 IPF and DTT releases do not apply (Windows 11 only); Windows 10 machines only get older OEM-supplied DTT builds.
- esif_uf is a debug shell inside the OEM-supplied DTT package; its availability depends on the OEM driver, and its commands and rights are undocumented by Intel. A kernel-side participant model suggests administrator rights, but that is not confirmed. The ESIF_WS directory name and the `web start` command suggest an embedded web server, but I found no documented address, and nothing says it ships or is enabled in current Windows builds.

### Gaps
- No Intel document for esif_uf commands, rights, or its 2026 availability on Windows; the only description is an unofficial personal page.
- No documentation for any IPF "client" or "ipf_sdk"; the terms appear only on the unofficial ESIF page (ipfclient) or not at all.
- The Intel Software License Agreement text was not read, so redistribution limits for IPF/DTT packages are unknown.
- Whether IPF or DTT exposes power in watts to non-Intel applications beyond the already-known EsifDeviceInformation WMI class is unknown.
- The IPF System Data release-note PDFs could not be fetched directly (HTTP 403).

## 3. AMD: Platform Management Framework (PMF) driver, Ryzen Master Monitoring SDK (package power, rights, licence), uProf power profiling on Windows (driver, rights)

### Takeaway
The Ryzen Master Monitoring SDK (v3.0.1.4971, Feb 2026) is the only AMD vendor-signed, documented user-mode API I found that returns package power (PPT in watts), but its licence is restrictive and I could not confirm the admin requirement. uProf provides power timecharts through its own kernel driver inside a profiler tool, not a library. The Windows PMF driver has no public telemetry interface.

### Cited Findings
Ryzen Master Monitoring SDK
- AMD's SDK page, as seen in search summaries (direct fetch timed out three times): latest 3.0.1.4971, 113 MB, dated 02/12/2026, Windows 10 and 11 64-bit; read-only SDK with about 20 API calls covering about 35 functions; metrics include EDC (A), TDC (A), PPT (W), average core voltage, temperature, voltage, CPU active state and frequencies; GetCPUParameters should be called at most once per second to avoid loading the SMU; when reinstalling to the same custom path the old folder must be deleted first. (search summary only; 2026) — [AMD Ryzen Master Monitoring SDK](https://www.amd.com/en/developer/ryzen-master-monitoring-sdk.html)
- Licence and redistribution: the RyzenMasterBindings author (MIT-licensed wrapper) says the SDK licence (shipped as License.rtf) is restrictive and he cannot redistribute the SDK header and library files, so each user must download the SDK from AMD. The wrapper page does not state driver, admin or metric details. (direct) — [RyzenMasterBindings](https://github.com/L3tum/RyzenMasterBindings)
- Security history: AMD-SB-9004 (rated High) covers incorrect default permissions in the SDK installation directory (CVE-2024-21945, SDK versions before 2.13.0.2915) and in the Ryzen Master utility (CVE-2024-21946); CVE records published 12 Nov 2024, patch 18 Dec 2024. (search summary only; older: 2024) — [AMD-SB-9004](https://www.amd.com/en/resources/product-security/bulletin/amd-sb-9004.html), [CVE-2024-21945 record](https://cveawg.mitre.org/api/cve/CVE-2024-21945)

uProf power profiling
- uProf quick-reference guide (PDF read locally; the Linux paths in it name the 6.0.x release line): power profiling (power, thermal and frequency behaviour of the system) is listed for Windows and Linux, reported through the timechart command, example `AMDuProfCLI timechart -e power -t 500 -d 10 -o <dir>`. On Linux the uProf driver is installed once and root is then not required for collection; HSMP-based power metrics (--collect-power) need the amd_hsmp module or the uProf driver on Zen 4/Zen 5 (server parts). The guide has no Windows driver or administrator notes. (2026) — [uProf quick reference](https://docs.amd.com/api/khub/documents/nveV2jevKcum4LePBqpt3w/content)
- AMD's user guide says the power-profiling driver must be installed and running before the uProf power APIs work. (search summary only) — [uProf user guide, Using the APIs](https://docs.amd.com/r/en-US/57368-uProf-user-guide/Using-the-APIs)
- A 2020 community post shows the Windows GUI installer calling `Driver_Install.bat -i` in the uProf bin folder (and failing). (search summary only; older: 2020) — [AMD community thread](https://community.amd.com/t5/server-processors/issues-installing-and-running-amd-uprof/td-p/79734)
- An AMD staff reply on a Linux thread says AMDuProfPcm does not support client parts (date not shown). (search summary only) — [AMD community thread](https://community.amd.com/t5/pc-drivers-software/amduprofpcm-says-missing-configuration-file-unsupported/m-p/541092)
- Driver security history: CVE-2021-26334 (AMDPowerProfiler.sys let lower-privileged users read MSRs in the kernel, older: 2021), an IOCTL-validation issue (CVE-2023-20562, could allow loading an unsigned driver), and AMD-SB-9022 (file-write privilege escalation, CVE-2025-61969). (search summary only) — [AMD-SB-1016](https://www.amd.com/en/corporate/product-security/bulletin/amd-sb-1016), [CVE-2023-20562 (Red Hat)](https://vulners.com/redhatcve/RH:CVE-2023-20562), [AMD-SB-9022](https://www.amd.com/en/resources/product-security/bulletin/AMD-SB-9022.html)

Platform Management Framework (PMF)
- The Windows "AMD PMF Device Driver" supports 64-bit Windows 10 and 11 and appears as "AMD PMF Driver" in Device Manager; the README of version 23.2.3.0 is minimal and documents no telemetry interface (third-party mirror of AMD's README). (search summary only) — [README mirror](https://mdt.dragonfly.pl/Out-of-Box%20Drivers/System/amdpmf_23.2.3.0_51E1F9D04475C6D5EEAEB05104B88AE9DF2F44E34CDDAF77B3601D42DFD1593B/README.txt)
- AMD chipset driver 8.01.20.513 bundles PMF packages by series (PMF-6000, 7040, 8000, Ryzen AI 300). (search summary only) — [Neowin](https://www.neowin.net/news/amd-releases-new-windows-1110-chipset-driver-for-ryzen-9000-7000-5000-3000-and-more/)
- Linux amd_pmf shows what the framework uses internally: sensor inputs and APU metrics, with socket power computed as apu_power plus dgpu_power from a metrics table, and a util character device that exposes metrics to user space on Linux. (search summary only) — [Linux amd_pmf core.c](https://sources.debian.org/src/linux/6.12.8-1/drivers/platform/x86/amd/pmf/core.c), [amdpmf-interface ABI](https://kernelsources.org/source/xref/linux/Documentation/ABI/testing/amdpmf-interface)

### Inferences
- PMF: the power data exists inside AMD's stack, but on Windows there is no documented API, so it is not a path.
- The Monitoring SDK is AMD's closest equivalent of the retired Intel Power Gadget API: read-only, about 1 Hz, PPT in watts, driver-backed, AMD-only, with a licence that prevents bundling AMD's files in an open-source or redistributed tool.
- uProf is a profiling product with its own kernel driver; for an energy-measurement program it would mean installing and driving AMD's tool, with an unconfirmed admin requirement on Windows and no confirmed Ryzen client support in the PCM-style tool.

### Gaps
- The AMD SDK page fetch timed out three times (and AMD-SB-1052 / AMD-SB-9004 once each), so SDK details are search-summary only; the EULA text was not read.
- Not found: whether calling the SDK needs administrator rights; the name and signing level of the driver it installs; the supported CPU list (desktop and mobile Ryzen); whether PPT is measured or modelled by the SMU. One unnamed third-party guide claims the full Ryzen Master installer should run as administrator because it bundles the driver; that is not an AMD source and not about the SDK.
- uProf on Windows: administrator requirement, driver signing level, EULA and redistribution, and whether the power timechart works on Ryzen client CPUs were not found.
- PMF: no Windows telemetry API found; absence of documentation only.

## 4. Maker WMI interfaces on business PCs: Dell Command | Monitor, HP (InstrumentedBIOS numeric sensors, Performance Advisor, Sure Admin), Lenovo (Vantage, Lenovo WMI): any real power readings?

### Takeaway
No source shows a measured-watts WMI sensor on a Dell, HP or Lenovo business PC. Dell Command | Monitor exposes voltage, current, temperature and fan sensors plus battery inventory; HP's BIOS WMI numeric sensors seen in the wild are temperature, fan and chassis intrusion; Lenovo's WMI exposes power limits (settings), not measurements.

### Cited Findings
Dell
- Dell Command | Monitor FAQ: uses the root/dcim/sysman namespace; supports Venue 11 Pro, OptiPlex, Precision and Latitude client systems; DCIM_NumericSensor covers voltage, current, temperature and fans; DCIM_Battery gives battery inventory and monitoring; sensor monitoring covers voltage, temperature, amperage, fans and chassis; no mention of power consumption or watts; administrator rights on the client are listed as a prerequisite (and wbemtest should be run as administrator). The page names version 9.0 as latest, but the article was last modified 19 Aug 2025, which conflicts with the 10.x guides below, so it is stale. (direct) — [Dell Command | Monitor FAQ](https://dell.com/support/kbdoc/en-lr/000177297/dell-command-monitor-faq)
- Dell publishes Command | Monitor 10.x reference guides (10.1.0, 10.2.1, 10.4, 10.6, 10.7) that contain DCIM_Battery and DCIM_NumericSensor class sections; the 10.7 manual lists DCIM_Battery, DCIM_Processor, DCIM_NumericSensor and DCIM_Sensor among supported classes. (search summary only; 2026-era versions) — [Reference Guide 10.2.1](https://dl.dell.com/topicspdf/command-monitor-v1021_Reference-Guide_ja.pdf)
- Dell's sensors profile (DMTF-style, server oriented) separates power numeric sensors (DCIM_PSNumericSensor) from non-power analog sensors (DCIM_NumericSensor), and Dell's BaseMetricsProfile defines maximum, minimum and average power consumption in watts. These are profile documents, not evidence that Command | Monitor shows them on clients. (search summary only) — [Dell sensors profile 1.1](https://dl.dell.com/manuals/common/dell_sensorsprofile-1.1.pdf), [Dell BaseMetricsProfile 1.0](https://dl.dell.com/manuals/common/Dell_BaseMetricsProfile_1.0.pdf)
- The Dell Command | Monitor user guide PDF returned HTTP 403, so class property lists were not read.

HP
- The Linux hp-wmi-sensors documentation covers HP business-class computers that publish monitoring data through BIOS WMI. The kernel exposes current, fan, voltage and temperature sensors plus chassis intrusion, and says only temperature, fan speed and intrusion sensors have so far been observed on real machines; voltage and current support is provisional; the WMI class is named HPBIOS_BIOSNumericSensor, objects vary between systems, and inconsistent BIOS implementations or dubious sensor hardware can give inaccurate readings; watt sensors are not mentioned. Events use class HPBIOS_BIOSEvent in root\wmi. (direct) — [hp-wmi-sensors documentation](https://docs.kernel.org/hwmon/hp-wmi-sensors.html)
- The driver source comments name the class HP_BIOSNumericSensor (inheriting HP_BIOSSensor) and list Watts and Joules among the unit codes, but the driver maps only temperature, voltage, current and airflow sensors to hwmon, so a watts unit exists in the schema but nothing publishes it in practice. The two sources spell the class differently (HPBIOS_ vs HP_BIOS). (search summary only) — [hp-wmi-sensors.c](https://codebrowser.dev/linux/linux/drivers/hwmon/hp-wmi-sensors.c.html)

Lenovo
- The Linux lenovo-wmi-other driver documentation says the capability data interface provides information on power limits of the integrated CPU and GPU, exposed as settings ppt_pl1_spl, ppt_pl2_sppt and ppt_pl3_fppt. These are limits to set, not measurements. (search summary only; 2026-era kernel docs) — [lenovo-wmi-other documentation](https://kernel.org/doc/html/v6.19-rc1/wmi/devices/lenovo-wmi-other.html)
- The community Legion Toolkit exposes Custom Mode power limits (PL1/PL2) and presents itself as a replacement for features found only in Lenovo Vantage or Legion Zone; no source describes a measured-wattage WMI call. (search summary only) — [Legion Toolkit](https://sourceforge.net/app/lenovo-legion-toolkit/)

### Inferences
- On business PCs the maker WMI layers are not a source of measured watts. The realistic whole-machine measured signal on a laptop is the battery rate (section 6); on desktops nothing was found.
- HP's schema includes a Watts unit code, so a BIOS could in theory publish a power sensor, but none was reported in the wild according to the kernel documentation.

### Gaps
- HP: the namespace root\HP\InstrumentedBIOS holding numeric sensors was not confirmed in any source; no source found for HP Performance Advisor or HP Sure Admin describing any power telemetry (not researched in depth).
- Lenovo Vantage / Commercial Vantage and ThinkPad WMI: no source found on any power reading.
- Dell: the full property list of DCIM_NumericSensor and DCIM_Battery in the current 10.x guide, whether any instance reports watts on laptops, the WMI query rights, and the licence of Command | Monitor were not verified (PDFs returned 403).
- Vendor charger-wattage displays (Dell Power Manager, HP Support Assistant, Lenovo Vantage) were not researched.

## 5. ACPI 4.0 power meters (ACPI000D, acpipmi.sys, "Power Meter" counter set): which machines actually have them

### Takeaway
Windows 7 and later ship the inbox driver ACPIPMI.SYS and a user-mode service that publish ACPI 4.0 power meters through the WMI class Win32_PowerMeter (root\cimv2\power, watts), but only where the firmware implements the ACPI 4.0 power-metering objects (hardware ID ACPI000D). Microsoft's wording points at power supplies and baseboard management controllers, so servers; I found no list of any desktop, workstation or laptop models that have one, and no documentation of a "Power Meter" counter set.

### Cited Findings
- Windows 7 / Server 2008 R2 introduced the Power Metering and Budgeting infrastructure. A power meter is a hardware component typically provided by a power supply or a baseboard management controller; it can meter the whole system or a subsystem, and several meters can exist in one machine. (direct; docs dated 2017, updated 2025) — [Overview of the Power Metering and Budgeting Infrastructure](https://learn.microsoft.com/en-us/windows-hardware/drivers/powermeter/overview-of-the-power-metering-and-budgeting-infrastructure)
- ACPIPMI.SYS is the inbox driver that implements the Power Meter Interface for systems with ACPI 4.0 power-metering objects, so OEMs need no third-party driver; the firmware must implement those objects and the implementation details are proprietary and specific to each system; the ACPI objects abstract an underlying technology such as IPMI. (direct) — [Overview of the Power Metering and Budgeting Infrastructure](https://learn.microsoft.com/en-us/windows-hardware/drivers/powermeter/overview-of-the-power-metering-and-budgeting-infrastructure)
- The User-Mode Power Service exposes the data as WMI classes that follow DMTF Power Supply Profile 1.1.0, supports querying current values and callbacks when a threshold or budget is exceeded, and is used by PerfMon. (direct) — [User-Mode Power Service](https://learn.microsoft.com/en-us/windows-hardware/drivers/powermeter/user-mode-power-service)
- Win32_PowerMeter (namespace Root\CIMV2\power, provider UmPoWmi.dll, MOF PowerMeterProvider.mof, minimum client Windows 7): the primary WMI class for power consumption and budgeting; derived from CIM_NumericSensor; base units only watts with a UnitModifier; MeterType 0 = input power, 1 = output power; SamplingPeriod and Min/MaximumAveragingInterval in milliseconds; SupportCapabilities includes a flag for meters that report only while discharging, typical of mobile battery systems and some UPSs; a whole-system meter is associated with the Win32_ComputerSystem instance. The page is in the archived (previous-versions) docs. (direct; older: 2018, updated 2021) — [Win32_PowerMeter class](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/powermeterprov/win32-powermeter)
- ACPI hardware ID ACPI000D marks power supplies and meters that comply with the Power Meter Interface; an ACPI control method reads the current power draw. (search summary only) — [Overview of the ACPI 4.0 Power Metering Objects](https://msdn.microsoft.com/en-us/library/windows/hardware/ff543852%28v=vs.85%29.aspx)
- The Linux hwmon driver acpi_power_meter binds to ACPI000D, which confirms the ID is the standard one. It does not say which products have the device. (search summary only) — [acpi_power_meter.c](https://codebrowser.dev/linux/linux/drivers/hwmon/acpi_power_meter.c.html)

### Inferences
- Expect zero Win32_PowerMeter instances on typical Windows 10/11 desktops, workstations and laptops; the documented sources point at servers (BMC/IPMI-backed supply meters). That is an inference from Microsoft's wording, not from a hardware survey.
- A quick, read-only check on any machine (my suggestion, not from a source): list instances of Win32_PowerMeter in root\cimv2\power and look for a device with hardware ID ACPI000D in Device Manager.

### Gaps
- No evidence found for any specific shipping Windows server, workstation or laptop model that exposes ACPI000D; no public list.
- No documentation found for a PerfMon counter set named "Power Meter" or its counter names; the Microsoft page only says PerfMon uses the data.
- Rights needed to query root\cimv2\power are not stated on the pages read.
- Accuracy, resolution and refresh of real ACPI meters were not researched.

## 6. USB-C Power Delivery: does Windows expose negotiated or measured charger power to programs (UCSI, Windows.Devices.Power, WMI)?

### Takeaway
No. The negotiated PD contract stays inside the inbox UCM-UCSI client or a vendor UcmCx driver, and Windows shows its own slow-charging toast; Microsoft requires that these notifications come from Windows rather than third-party software. The closest in-box measured signals are battery-side power rates, which equal whole-machine draw only while the machine discharges.

### Cited Findings
- Microsoft's slow-charging requirements: on UCSI machines the inbox UCM-UCSI ACPI client driver manages Type-C connectors (recommended path); for non-UCSI hardware a UcmCx client driver reports the negotiated contract (the RDO) to the OS; UCSI GET_CONNECTOR_STATUS carries a battery charging capability status (not charging, nominal, slow, very slow) and a status-change bit; slow charging is reported only when the connector is a power consumer. Microsoft's certification requirement says Type-C charging notifications must rely on built-in Windows notifications and "not be provided by third-party software". (direct; page 2024, updated 2025) — [USB Type-C slow charging notification requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-type-c-slow-charging-notification-requirements)
- The UCSI HLK tests include a "Negotiated Power Level Change" notification test, i.e. the contract change is a driver-level event. (search summary only) — [UCSI Get Connector Status - Negotiated Power Level Change](https://learn.microsoft.com/en-us/windows-hardware/test/hlk/testref/a32aaf5a-2277-4894-964e-fd3409719fb0)
- Windows.Devices.Power.BatteryReport (Windows 10 build 10240 and later) offers ChargeRateInMilliwatts, DesignCapacity, FullChargeCapacity and RemainingCapacity in energy units and a status value; every property is null when no battery is present or the battery controller does not report the value. The namespace covers batteries only. (direct) — [BatteryReport class](https://learn.microsoft.com/en-us/uwp/api/Windows.Devices.Power.BatteryReport)
- SYSTEM_BATTERY_STATE.Rate is the current charge or discharge rate in mW: positive = charging, negative = discharging; some batteries report only discharge rates; minimum client Windows XP. The page does not say how the structure is retrieved or whether administrator rights are needed. (direct) — [SYSTEM_BATTERY_STATE](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_battery_state)
- WMI class BatteryStatus in root\wmi has ChargeRate and DischargeRate in mW (BATTERY_WMI_STATUS: ChargeRate, DischargeRate, RemainingCapacity, Voltage, PowerOnline); a community example filters on Voltage > 0 to find the real battery instances. (search summary only) — [BATTERY_WMI_STATUS](https://learn.microsoft.com/en-us/windows/win32/api/batclass/ns-batclass-battery_wmi_status), [TechNet Battery Status Object](https://technet.microsoft.com/en-us/library/cc780145(v=ws.10).aspx)
- Vendor support varies: in one thread a user reports HP saying DischargeRate was not supported on that model. (search summary only; anecdote) — [AskWoody thread](https://www.askwoody.com/forums/topic/wmi-battery-status-showing-discharge-rate-of-large-negative-number)
- A consumer how-to (low-quality source) states that Windows 11 exposes no universal live USB PD wattage reading and recommends an inline USB-C meter. (search summary only) — [How to Check USB-C Charging Wattage in Windows 11](https://thunderboltlaptop.com/?p=999690)

### Inferences
- Charger wattage and the PD contract are not available to applications through any documented Windows API; only a slow-charging verdict is surfaced, and only in Windows' own UI.
- Battery rate is the usable proxy: on battery, the absolute value approximates whole-machine draw (display and all), limited by the fuel-gauge accuracy and update rate; on AC it gives only the power flowing into the battery, not charger or machine power.
- A read-only way to try it on a laptop (my suggestion): query root\wmi BatteryStatus, or call BatteryReport, while unplugged.

### Gaps
- Absence of evidence only: no Microsoft documentation of a user-mode API or WMI class for the PD contract was found; the root\wmi classes for UCSI were not enumerated.
- Update interval and accuracy of battery rates, and which laptops report them, were not found beyond the null-value remark and one HP anecdote.
- Whether these battery interfaces need administrator rights is not stated in the pages read.
- How to retrieve SYSTEM_BATTERY_STATE (CallNtPowerInformation) was not confirmed from the page fetched.

## 7. PawnIO (used by LibreHardwareMonitor): signing level, Microsoft vulnerable-driver blocklist status, administrator needs, licence

### Takeaway
PawnIO is a third-party driver by namazso, not in-box and not OEM-signed. Its official build is described as signed, but the site and repositories do not say whether that is WHQL or attestation; no source shows it on Microsoft's blocklist, but I could not read the blocklist contents either. Device access is administrator-only by default (2.2.0 added an optional non-admin exposure, which the author advises against). The driver is GPL-2.0 with a linking exception, the modules LGPL-2.1, and a custom licence is offered.

### Cited Findings
- pawnio.eu describes PawnIO as a scriptable universal kernel driver (Pawn scripting language) that lets software implementing PawnIO modules reach hardware. Four distributions: Official (signed, identical to the open-source build), Unrestricted (unsigned, no signature checking, selectable in the installer), Open-source, and Licensed (custom licence on request). Provided as-is. No version, date, signing type, rights or blocklist statement. (direct) — [pawnio.eu](https://pawnio.eu)
- Driver repository: GPL-2.0, copyright 2026 namazso, with a linking exception for programs that talk to the driver only through the device IO-control interface (the exception does not cover programs that use the Pawn interface) and a requirement that loaded modules be licence-compatible; downloads are at pawnio.eu; the repository has PawnIO, PawnIOLib, PawnIOUtil and pawn folders. (direct) — [namazso/PawnIO](https://github.com/namazso/PawnIO)
- Modules repository: official modules include IntelMSR, AMDFamily17, RyzenSMU, IntelMCHBAR, IntelOOBMSM, IsaBridgeEC, LpcIO, SmbusI801, SmbusPIIX4, SmbusNCT6793, DellSMM, Nvidia, among others; licence LGPL-2.1; signed builds are published in Releases. The page does not say what MSRs IntelMSR allows (RAPL not mentioned). (direct) — [namazso/PawnIO.Modules](https://github.com/namazso/PawnIO.Modules)
- Signing evidence: PawnIO.Setup issue #1 (opened 19 Sep 2025): FACEIT anti-cheat refused to whitelist PawnIO because its signing certificate was also used by cheat software; the maintainer agreed and planned to get a unique certificate, noted that OV certificates are available to individuals (the one on his user-mode binaries) while EV certificates require an LLC or equivalent and that he would partner with an existing LLC or start one; reported fixed as of version 2.1.0 on 12 Feb 2026; locked as solved on 24 May 2026. No mention of WHQL, attestation or the blocklist. (direct) — [PawnIO.Setup issue #1](https://github.com/namazso/PawnIO.Setup/issues/1)
- Microsoft's attestation signing requires at least one EV certificate on the Hardware Dev Center account; attestation-signed drivers cannot be published to Windows Update. (search summary only) — [Microsoft Learn, attestation signing](https://learn.microsoft.com/ga-ie/Windows-hardware/drivers/dashboard/code-signing-attestation)
- PawnIO.Setup releases: 2.2.0 (15 Mar; year not shown) made it possible to expose the device to non-administrators (which the author advises against), and returns a reboot-required code in silent mode; 2.1.0 (12 Feb) switched driver installation to the Device API; 2.0.1 (29 Jun) fixed DLL hijacking of CRYPTBASE.dll in the installer; 2.0.0 (24 May) was a new installer. (direct) — [PawnIO.Setup releases](https://github.com/namazso/PawnIO.Setup/releases)
- LibreHardwareMonitor releases page (truncated at 100k of 132k characters): v0.9.6 (14 Feb; year not shown) updated PawnIO modules to 2.2 and fixed things for a new PawnIO release and installer; v0.9.5 (7 Jan) shows no PawnIO entry in the part I could read. The README says LibreHardwareMonitor is MPL-2.0 and that some sensors require administrator privileges (restart the host with admin rights or add a requireAdministrator manifest). (direct) — [LibreHardwareMonitor releases](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases), [LibreHardwareMonitor README](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor)
- Another source says LibreHardwareMonitor must be installed alongside the separately installed PawnIO driver, and FanControl moved to the PawnIO build of LHM so WinRing0 is no longer shipped. (search summary only) — [FanControl discussion](https://github.com/Rem0o/FanControl.Releases/discussions/3017)
- Defender context: WinRing0 is detected as HackTool:Win32/Winring0 (also VulnerableDriver:WinNT/Winring0.G) because it can be exploited and is unpatched; I found no report of Defender flagging PawnIO. (search summary only) — [Slashdot report on Defender flagging WinRing0](https://it.slashdot.org/story/25/03/14/1351225/windows-defender-now-flags-winring0-driver-as-security-threat-breaking-multiple-pc-monitoring-tools)
- Microsoft's driver blocklist page (updated 8 Oct 2026): the vulnerable driver blocklist is on by default since the Windows 11 2022 update and enforced with memory integrity (HVCI), Smart App Control or S mode, updated quarterly and through monthly Windows updates; a downloadable fuller list is offered. The fetched text does not list individual drivers, so PawnIO's presence or absence could not be checked. (direct; 2026) — [Microsoft recommended driver block rules](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/microsoft-recommended-driver-block-rules)

### Inferences
- Signing level: Microsoft requires an EV certificate for attestation signing, and the maintainer said EV needs an LLC and that he would partner with one, so the official driver is plausibly attestation-signed through a partner account; this is not documented and not proven (it could also be WHQL).
- Blocklist: no evidence of a listing, but also no positive confirmation; the list is hash and certificate based and quarterly. A signing-certificate problem (FACEIT, 2025) was reported and said to be fixed from 2.1.0, which means the certificate history is a risk factor worth re-checking.
- Rights: the default device ACL appears to be administrator-only (the non-admin option in 2.2.0 is a deliberate change), and installing a kernel driver needs elevation; LibreHardwareMonitor itself needs elevation for privileged sensors. So reading CPU package power through PawnIO means an elevated process unless the device is deliberately exposed.
- Licence for an app: GPL-2.0 with a linking exception for IOCTL clients, modules LGPL-2.1; bundling or redistributing the signed installer inside a product is probably possible under those terms, but the author also sells a custom licence; confirm before shipping.
- PawnIO is a third-party driver, not a vendor-signed or in-box path; its value is that signed modules restrict what the driver can do, unlike WinRing0's generic port and MSR access (inference from the site's description of signature-checked modules and an unrestricted variant).

### Gaps
- The exact signing chain (WHQL vs attestation, signer name) is not documented; check the installed PawnIO driver's signature locally (not done: read-only research).
- Blocklist status is unconfirmed: the Microsoft page text did not list drivers, and LOLDrivers was not checked directly.
- Whether the IntelMSR and AMDFamily17 modules allow the RAPL energy registers (and which ones LibreHardwareMonitor uses for package power) was not found in the README-level pages.
- Years are missing on the GitHub release dates; the match with the 12 Feb 2026 issue comment suggests 2.1.0 = Feb 2026 and 2.2.0 = Mar 2026, but that is inferred.
- The user states LibreHardwareMonitor 0.9.5 and later use PawnIO; the readable part of the releases page shows PawnIO only in 0.9.6.

## 8. Overall: which of these paths could an independent Windows 10/11 program rely on in Nov 2026?

### Takeaway
Only three documented, current paths give a user-mode program measured power without third-party reverse engineering: AMD's Ryzen Master Monitoring SDK (AMD CPUs, restrictive licence), the battery-rate interfaces (laptops, on battery only), and Win32_PowerMeter (only machines with ACPI000D, mostly servers). Intel's PMT/IPF/DTT stack is Intel-tools-only, the Dell/HP/Lenovo layers give no watts, USB-C PD is not exposed, and PawnIO is a third-party driver that needs an elevated process.

### Cited Findings
- No new sourced facts here: every cell below is derived from the cited findings in sections 1 to 7, where the sources are given.

### Inferences
Summary matrix (derived from sections 1 to 7; "?" means not found):

| Path | What it measures | Hardware and OS | Rights | How a program reads it | Licence limit | State in 2026 | Verdict |
|---|---|---|---|---|---|---|---|
| Intel PMT driver (IntcPMT) | Platform telemetry counters (client examples: bandwidth, discrete-GPU power); package energy not confirmed | Tiger Lake onward, OEM drivers; Windows 10/11 for SoC Watch | SoC Watch needs admin; device ACL ? | No public API; Intel SoC Watch or IPF PMT provider only | ? | Driver shipped by OEMs for 2024 and 2026 platforms | Intel-tools-only |
| Intel IPF and providers | Platform data for select Intel software; Device Management providers for management apps | Core Ultra and Core 12th gen+, Windows 11 only | Install needs admin; use ? | Namespace-based API; no SDK or docs published | Intel Software License Agreement | IPF 2.3.20306.4 (Jul 2026) | Not available to independent apps |
| Intel DTT, esif_uf shell | Participant data (unofficial: energy counter, PMAX) | OEM DTT driver; 2026 builds Windows 11 only | ? | Shell commands (unofficial docs); known WMI class excluded | ? | DTT 9.1.10011.2963 (Jul 2026); DPTF repo archived Aug 2026 | Fragile, undocumented |
| AMD Ryzen Master Monitoring SDK | PPT (W), EDC, TDC, voltage, temperature, frequency | Ryzen, Windows 10/11 x64 | ? (no SDK-specific admin rule found) | API calls, at most once per second | AMD licence, files not redistributable by third parties | 3.0.1.4971 (Feb 2026) | Usable if the licence fits; AMD only |
| AMD uProf | Power, thermal, frequency timechart | Windows and Linux; own kernel driver | ? on Windows | CLI timechart / GUI | ? | 6.0.x line | A profiler, not a library |
| AMD PMF driver | None exposed | Ryzen mobile and desktop | n/a | No public interface on Windows | n/a | PMF packages in chipset driver | Not a path |
| Dell Command or Monitor | No watts found (voltage, current, temperature, fan, battery inventory) | Dell OptiPlex, Precision, Latitude | Admin prerequisite listed | WMI root/dcim/sysman | ? | 10.x guides exist | No |
| HP BIOS WMI numeric sensors | Temperature, fan, intrusion in the wild | HP business PCs | ? | WMI | n/a | Linux docs, undated | No |
| Lenovo WMI | Power limits (settings) | Legion and similar | ? | WMI | n/a | 2026-era kernel docs | No |
| ACPI 4.0 power meter via ACPIPMI.SYS | Watts, input or output, whole system or per supply | Only firmware with ACPI000D (servers, BMC-backed) | ? | WMI Win32_PowerMeter in root\cimv2\power | In-box | Docs from 2017/2018, still live | Only where present |
| USB-C PD contract (UCSI, UcmCx) | Negotiated contract | Type-C laptops | Kernel only | Not exposed; only a slow-charging toast | n/a | Documented 2024/2025 | No |
| Battery rate (BatteryReport, SYSTEM_BATTERY_STATE, WMI BatteryStatus) | Battery charge or discharge power in mW | Laptops whose controller reports it | Not stated | WinRT, Win32 or WMI | In-box | Current API (Windows 10 build 10240+) | Whole-machine power on battery only |
| PawnIO with LibreHardwareMonitor | Raw MSR and similar via signed modules | Intel and AMD, Windows | Admin by default | PawnIOLib IOCTL; or LHM | GPL-2.0 plus exception; modules LGPL-2.1; custom licence offered | 2.2.0 (Mar 2026); signing level undocumented | Third-party, not vendor-signed or in-box |

- Windows 10 caveat: the 2026 Intel IPF and DTT releases are Windows 11 only; the AMD SDK states Windows 10 and 11; the ACPI, battery and WMI paths are the same on both.

### Gaps
- Source quality: many AMD, Intel PDF and Dell pages could not be fetched (403 or timeout), so several findings rest on search-engine summaries; each is tagged. The HP softpaq results on a trycloudflare.com mirror were deliberately not used.
- No source found for rights (admin or standard user) on Win32_PowerMeter, the battery interfaces, IntcPMT, IPF or the AMD SDK; these should be tested on a real machine if the decision depends on them.
- GPU power paths (NVML, ADLX, IGCL) are outside this assignment and were not researched.
