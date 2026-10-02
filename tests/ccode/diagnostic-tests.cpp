#include "../../scripts/ccode/diagnostics.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace ccode;

    assert(NeutralErrorCode("E_GATEWAY_AUTH: token=do-not-record") == "E_GATEWAY_AUTH");
    assert(NeutralErrorCode("E_SESSION_BUSY") == "E_SESSION_BUSY");
    assert(NeutralErrorCode("private C:\\Users\\person\\secret.txt") == "E_LOCAL");
    assert(NeutralErrorCode("E_bad: secret") == "E_LOCAL");
    assert(NeutralErrorCode("E_PRIVATE_TOKEN_12345: hidden") == "E_LOCAL");
    assert(NeutralErrorCode("E_GATEWAY_TLS_PRIVATE_HOST: hidden") == "E_LOCAL");
    assert(NeutralErrorCode("E_GATEWAY_DNS") == "E_GATEWAY_DNS");
    assert(DiagnosticCategory("E_GATEWAY_DNS") == "network");
    assert(DiagnosticCategory("E_GATEWAY_TLS") == "network");
    assert(DiagnosticCategory("E_PROTOCOL_JSON") == "protocol");
    assert(DiagnosticCategory("E_MISSING_RESULT") == "protocol");
    assert(DiagnosticCategory("E_TRUNCATED_EVENT") == "protocol");
    assert(DiagnosticCategory("E_MISSING_RESULT_PRIVATE") == "local");
    assert(DiagnosticCategory("E_CREDENTIAL") == "configuration");
    assert(DiagnosticCategory("E_SESSION_BUSY") == "concurrency");
    assert(DiagnosticCategory("E_PACKAGE_METADATA") == "integrity");
    assert(DiagnosticCategory("E_PROFILE_DATA") == "data");
    assert(DiagnosticCategory("E_ACTIVE_PROFILE") == "data");
    assert(DiagnosticCategory("E_ACTIVE_PROFILE_PRIVATE") == "local");
    assert(DiagnosticCategory("E_LOCAL") == "local");

    const std::string operation = "a2345678-1234-4234-8234-123456789abc";
    const auto report = FailureDiagnostic(
        "E_GATEWAY_AUTH: token=do-not-record prompt=private C:\\Users\\person", 64, operation);
    assert(report == Json({
        {"schemaVersion", 1},
        {"product", "ccode"},
        {"platform", "windows"},
        {"architecture", "x64"},
        {"status", "error"},
        {"operationId", operation},
        {"errorCode", "E_GATEWAY_AUTH"},
        {"category", "network"},
        {"exitCode", 64},
        {"privacy", {
            {"argumentsCaptured", false},
            {"environmentValuesCaptured", false},
            {"promptOrContentCaptured", false},
            {"credentialsCaptured", false}
        }}
    }));
    const auto checksum = FailureDiagnostic(
        "E_CHECKSUM: private payload path and token", 64, operation);
    assert(checksum["errorCode"] == "E_CHECKSUM");
    assert(checksum["category"] == "integrity");
    assert(checksum["exitCode"] == 64);
    assert(checksum["operationId"] == operation);
    assert(checksum.dump().find("private") == std::string::npos);
    const auto runtimeIntegrity = FailureDiagnostic(
        "E_RUNTIME_INTEGRITY: private payload path and token", 64, operation);
    assert(runtimeIntegrity["errorCode"] == "E_RUNTIME_INTEGRITY");
    assert(runtimeIntegrity["category"] == "integrity");
    assert(runtimeIntegrity.dump().find("private") == std::string::npos);
    assert(NeutralErrorCode("E_RUNTIME_INTEGRITY_PRIVATE_TOKEN") == "E_LOCAL");
    for (const auto* code : {"E_MANIFEST_DIRECTORY", "E_MANIFEST_DOCUMENT",
            "E_MANIFEST_FILE", "E_MANIFEST_FILE_LIMIT", "E_MANIFEST_FILE_MISMATCH",
            "E_MANIFEST_HASH", "E_MANIFEST_INVENTORY", "E_MANIFEST_PE",
            "E_MANIFEST_RESOURCE", "E_MANIFEST_SCHEMA", "E_MANIFEST_SIGNATURE"}) {
        const auto manifestFailure = FailureDiagnostic(
            std::string(code) + ": private path and token", 64, operation);
        assert(manifestFailure["errorCode"] == code);
        assert(manifestFailure["category"] == "integrity");
        assert(manifestFailure.dump().find("private") == std::string::npos);
    }
    assert(NeutralErrorCode("E_MANIFEST_SIGNATURE_PRIVATE_TOKEN") == "E_LOCAL");
    const auto unknown = FailureDiagnostic("E_PRIVATE_TOKEN_12345", 64, operation);
    assert(unknown["errorCode"] == "E_LOCAL");
    assert(unknown["category"] == "local");
    assert(unknown.dump().find("PRIVATE_TOKEN") == std::string::npos);
    const auto encoded = report.dump();
    for (const auto* forbidden : {"do-not-record", "private", "Users", "token="})
        assert(encoded.find(forbidden) == std::string::npos);

    assert(DiagnosticDestination({L"ccode.exe", L"--diagnostics", L"failure.json", L"--print"}) ==
           std::filesystem::path(L"failure.json"));
    assert(!DiagnosticDestination({L"ccode.exe", L"--model", L"--diagnostics", L"prompt"}));
    assert(!DiagnosticDestination({L"ccode.exe", L"--", L"--diagnostics", L"failure.json"}));
    assert(!DiagnosticDestination({L"ccode.exe", L"--diagnostics"}));

    bool invalidId = false;
    try { FailureDiagnostic("E_LOCAL", 64, "not-an-operation-id"); }
    catch (const std::runtime_error& error) {
        invalidId = std::string(error.what()) == "E_DIAGNOSTIC_ID";
    }
    assert(invalidId);

    std::cout << "ccode diagnostic privacy tests passed\n";
    return 0;
}
