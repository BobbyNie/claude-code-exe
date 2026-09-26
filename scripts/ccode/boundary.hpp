#pragma once
#include "frontend.hpp"

namespace ccode {
// This document is intentionally static and contains no environment values.
// It describes the public aliases we accept, the original runtime names the
// child engine still receives, and binary/notices scope that packaging must
// preserve rather than silently relabel.
inline Json BoundaryManifest() {
    return Json{
        {"schemaVersion", 1},
        {"platform", "windows"},
        {"architecture", "x64"},
        {"minimumWindowsBuild", 22000},
        {"publicEnvironment", {
            {"acceptedExact", Json::array({
                "A_API_KEY", "A_AUTH_TOKEN", "A_BASE_URL", "CCODE_DATA_DIR"
            })},
            {"acceptedPrefixes", Json::array({"A_", "C_"})},
            {"valuesRecorded", false},
        }},
        {"childRuntimeEnvironment", {
            {"inheritedFiltering", {
                {"removedPrefixes", Json::array({"ANTHROPIC_", "CLAUDE_"})},
                {"removedExact", Json::array({"CLAUDECODE"})},
            }},
            {"aliasExpansion", Json::array({
                Json{{"publicPrefix", "A_"}, {"runtimePrefix", "ANTHROPIC_"}},
                Json{{"publicPrefix", "C_"}, {"runtimePrefix", "CLAUDE_CODE_"}},
            })},
            {"profileRelative", {
                {"APPDATA", "roaming"},
                {"HOME", "home"},
                {"LOCALAPPDATA", "local"},
                {"TEMP", "temp"},
                {"TMP", "temp"},
                {"USERPROFILE", "home"},
            }},
            {"fixedValues", {
                {"CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC", "1"},
                {"CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK", "1"},
                {"CLAUDE_CODE_MAX_RETRIES", "0"},
                {"CLAUDE_CODE_RETRY_WATCHDOG", "0"},
                {"DISABLE_AUTOUPDATER", "1"},
            }},
            {"originalRuntimeNamesPresent", true},
            {"processTreeNameFree", false},
            {"valuesRecorded", false},
        }},
        {"binaryMetadata", {
            {"peResources", Json::array({
                Json{{"id", 101}, {"purpose", "opaque-embedded-engine"},
                     {"contentsScannedForNames", false}},
                Json{{"id", 102}, {"purpose", "validated-package-provenance-json"},
                     {"contentsScannedForNames", false}},
            })},
            {"publisherSignature", "not-asserted"},
            {"opaqueBinaryNameScan", "not-performed"},
        }},
        {"notices", {
            {"source", "enterprise-package-manifest"},
            {"launcherRewrites", false},
        }},
        {"sideEffects", {
            {"createsData", false},
            {"createsProfile", false},
            {"extractsRuntime", false},
        }},
    };
}
}  // namespace ccode
