// version.h: the one version of the whole energy suite, set at build time.
// Used by the service (status.json "service_version") and by ews-scan (first line of its summary).
// Build-Package.ps1 -Version passes it through Build-Native.ps1 -Version as the MSBuild property EwsVersion, which
// Directory.Build.props turns into the preprocessor value EWS_VERSION. A build without it reports 0.0.0.
#pragma once

/// Turns the value of a macro into a string literal.
#define EWS_STRINGIFY_VALUE(text) #text
#define EWS_STRINGIFY(text) EWS_STRINGIFY_VALUE(text)

#ifndef EWS_VERSION
#define EWS_VERSION 0.0.0
#endif

namespace ews {

/// The suite version text, for example "0.1.0".
inline constexpr const char* kSuiteVersion = EWS_STRINGIFY(EWS_VERSION);

}  // namespace ews
