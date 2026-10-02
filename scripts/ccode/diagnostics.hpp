#pragma once
#include "frontend.hpp"
#include <cctype>
#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace ccode {

inline bool HasPrefix(const std::string& value, const char* prefix) {
    return value.rfind(prefix, 0) == 0;
}

inline std::string NeutralErrorCode(const std::string& message) {
    if (!HasPrefix(message, "E_")) return "E_LOCAL";
    size_t end = 2;
    while (end < message.size()) {
        const unsigned char ch = static_cast<unsigned char>(message[end]);
        if (!(std::isupper(ch) || std::isdigit(ch) || ch == '_')) break;
        ++end;
    }
    if (end == 2) return "E_LOCAL";
    if (end < message.size() && message[end] != ':' && !std::isspace(static_cast<unsigned char>(message[end])))
        return "E_LOCAL";
    // Fixed product vocabulary only: arbitrary uppercase input can contain
    // credentials or private identifiers despite looking like an error code.
    static const std::set<std::string> knownCodes = {
        "E_ACTIVATION", "E_ACTIVATION_ALREADY_ACTIVE", "E_ACTIVATION_PENDING",
        "E_ACTIVATION_PENDING_INVALID", "E_ACTIVATION_PENDING_MISSING", "E_ACTIVATION_RECOVERY_EXISTS",
        "E_ACTIVATION_RECOVERY_ID", "E_ACTIVATION_RECOVERY_INVALID", "E_ACTIVATION_RECOVERY_WRITE",
        "E_ACTIVATION_SCOPE", "E_ACTIVATION_WRITE", "E_ACTIVE_PROFILE",
        "E_API_ONLY", "E_ARGUMENT", "E_CANCELLED",
        "E_CANDIDATE", "E_CANDIDATE_CHANGED", "E_CANDIDATE_DATA",
        "E_CANDIDATE_EXISTS", "E_CANDIDATE_HISTORY", "E_CANDIDATE_WORKSPACE",
        "E_CANDIDATE_WRITE", "E_CHECKSUM", "E_CREDENTIAL",
        "E_DATA", "E_DIAGNOSTIC_ID", "E_DIAGNOSTIC_WRITE",
        "E_DNS", "E_ENCODING", "E_ENGINE",
        "E_ENVIRONMENT", "E_EVENT", "E_EVENT_LIMIT",
        "E_EXTRACT", "E_EXTRACT_WRITE", "E_EXTRACT_ACTIVATE",
        "E_EXTRACT_ACCESS", "E_EXTRACT_SHARING", "E_EXTRACT_LOCKED", "E_EXTRACT_DISK_FULL", "E_GATEWAY", "E_GATEWAY_AUTH",
        "E_GATEWAY_RATE_LIMIT", "E_GATEWAY_RETRY", "E_GATEWAY_TLS", "E_GATEWAY_DNS",
        "E_HASH", "E_LOCAL", "E_LOCATION",
        "E_MANIFEST_DIRECTORY", "E_MANIFEST_DOCUMENT", "E_MANIFEST_FILE",
        "E_MANIFEST_FILE_LIMIT", "E_MANIFEST_FILE_MISMATCH", "E_MANIFEST_HASH",
        "E_MANIFEST_INVENTORY", "E_MANIFEST_PE", "E_MANIFEST_RESOURCE",
        "E_MANIFEST_SCHEMA", "E_MANIFEST_SIGNATURE",
        "E_MISSING_RESULT", "E_NETWORK", "E_NO_SESSION",
        "E_PACKAGE", "E_PACKAGE_METADATA", "E_PERMISSION_MODE",
        "E_PIPE", "E_PROCESS", "E_PROCESS_TREE",
        "E_PROFILE", "E_PROFILE_BUSY", "E_PROMPT",
        "E_PROTOCOL", "E_PROTOCOL_DUPLICATE_KEY", "E_PROTOCOL_FAILED",
        "E_PROTOCOL_JSON", "E_PROTOCOL_ORDER", "E_PROTOCOL_SCHEMA",
        "E_RESOURCE", "E_ROLLBACK", "E_ROLLBACK_CHANGED",
        "E_ROLLBACK_DATA", "E_ROLLBACK_EXISTS", "E_ROLLBACK_PATH",
        "E_ROLLBACK_WRITE", "E_RUNTIME", "E_RUNTIME_BUSY",
        "E_RUNTIME_PATH", "E_RUNTIME_INTEGRITY", "E_SESSION", "E_SESSION_BUSY",
        "E_SESSION_DATA", "E_SESSION_ID", "E_SESSION_INDEX_WRITE",
        "E_SESSION_LOCK", "E_SESSION_LOCK_PATH", "E_SESSION_MISMATCH",
        "E_SNAPSHOT", "E_SNAPSHOT_CHANGED", "E_SNAPSHOT_DATA",
        "E_SNAPSHOT_EXISTS", "E_SNAPSHOT_FS", "E_SNAPSHOT_HASH",
        "E_SNAPSHOT_ID", "E_SNAPSHOT_INTEGRITY", "E_SNAPSHOT_LINK",
        "E_SNAPSHOT_LOCATION", "E_SNAPSHOT_READ", "E_SNAPSHOT_TYPE",
        "E_SNAPSHOT_WRITE", "E_SOURCE_CHANGED", "E_START",
        "E_TLS", "E_TOOL", "E_TOOL_DUPLICATE_ID",
        "E_TOOL_DUPLICATE_NAME", "E_TOOL_ID", "E_TOOL_INPUT",
        "E_TOOL_NAME", "E_TOOL_UNKNOWN", "E_TRUNCATED_EVENT",
        "E_UNSUPPORTED_OPTION", "E_WORKSPACE", "E_WORKSPACE_DATA",
        "E_WORKSPACE_PATH", "E_WORKSPACE_PATH_TOO_LONG", "E_WORKSPACE_PENDING",
        "E_WORKSPACE_PENDING_INVALID", "E_WORKSPACE_PENDING_MISSING", "E_WORKSPACE_READ",
        "E_WORKSPACE_RECOVERY_EXISTS", "E_WORKSPACE_RECOVERY_ID", "E_WORKSPACE_RECOVERY_INVALID",
        "E_WORKSPACE_RECOVERY_WRITE", "E_WORKSPACE_UNSUPPORTED", "E_WORKSPACE_WRITE",
    };
    const auto code = message.substr(0, end);
    return knownCodes.count(code) ? code : "E_LOCAL";
}

inline std::string DiagnosticCategory(const std::string& errorCode) {
    if (HasPrefix(errorCode, "E_GATEWAY") || HasPrefix(errorCode, "E_NETWORK") ||
        HasPrefix(errorCode, "E_TLS") || HasPrefix(errorCode, "E_DNS")) return "network";
    if (HasPrefix(errorCode, "E_PROTOCOL") || HasPrefix(errorCode, "E_EVENT") ||
        HasPrefix(errorCode, "E_TOOL") || errorCode == "E_MISSING_RESULT" ||
        errorCode == "E_TRUNCATED_EVENT") return "protocol";
    if (errorCode.find("_BUSY") != std::string::npos || HasPrefix(errorCode, "E_SESSION_LOCK"))
        return "concurrency";
    if (HasPrefix(errorCode, "E_CREDENTIAL") || HasPrefix(errorCode, "E_ARGUMENT") ||
        HasPrefix(errorCode, "E_UNSUPPORTED_OPTION") || HasPrefix(errorCode, "E_PERMISSION_MODE") ||
        HasPrefix(errorCode, "E_API_ONLY") || HasPrefix(errorCode, "E_PROMPT")) return "configuration";
    if (HasPrefix(errorCode, "E_MANIFEST") || errorCode == "E_CHECKSUM" || HasPrefix(errorCode, "E_PACKAGE") || HasPrefix(errorCode, "E_RESOURCE") ||
        HasPrefix(errorCode, "E_HASH") || HasPrefix(errorCode, "E_RUNTIME") ||
        HasPrefix(errorCode, "E_CANDIDATE") || HasPrefix(errorCode, "E_SNAPSHOT") ||
        HasPrefix(errorCode, "E_ACTIVATION") || HasPrefix(errorCode, "E_ROLLBACK")) return "integrity";
    if (HasPrefix(errorCode, "E_PROFILE") || HasPrefix(errorCode, "E_SESSION") ||
        HasPrefix(errorCode, "E_WORKSPACE") || HasPrefix(errorCode, "E_DATA") ||
        HasPrefix(errorCode, "E_NO_SESSION") || errorCode == "E_ACTIVE_PROFILE") return "data";
    return "local";
}

inline bool ValidOperationId(const std::string& value) {
    if (value.size() != 36 || value[8] != '-' || value[13] != '-' ||
        value[18] != '-' || value[23] != '-' || value[14] != '4') return false;
    const auto hex = [](unsigned char ch) { return std::isxdigit(ch) != 0; };
    for (size_t i = 0; i < value.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        if (!hex(static_cast<unsigned char>(value[i]))) return false;
    }
    const unsigned char variant = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(value[19])));
    return variant == '8' || variant == '9' || variant == 'a' || variant == 'b';
}

inline std::optional<std::filesystem::path> DiagnosticDestination(const std::vector<std::wstring>& arguments) {
    const auto requiresValue = [](const std::wstring& option) {
        return option == L"--data-dir" || option == L"--workspace" ||
            option == L"--prepare-rollback" || option == L"--validate-rollback" ||
            option == L"--activate-rollback" || option == L"--stage-profile" ||
            option == L"--activate-profile" || option == L"--validate-profile" ||
            option == L"--model" || option == L"--max-turns" || option == L"--max-budget-usd" ||
            option == L"--allowedTools" || option == L"--disallowedTools" || option == L"--tools" ||
            option == L"--add-dir" || option == L"--plugin-dir" || option == L"--mcp-config" ||
            option == L"--settings" || option == L"--agent" || option == L"--agents" ||
            option == L"--append-system-prompt" || option == L"--permission-mode";
    };
    for (size_t i = 1; i < arguments.size(); ++i) {
        const auto& argument = arguments[i];
        if (argument == L"--") break;
        if (argument == L"--diagnostics") {
            if (i + 1 < arguments.size() && !arguments[i + 1].empty())
                return std::filesystem::path(arguments[i + 1]);
            return std::nullopt;
        }
        if (requiresValue(argument)) {
            if (i + 1 < arguments.size()) ++i;
            continue;
        }
        if ((argument == L"--resume" || argument == L"-r") && i + 1 < arguments.size() &&
            (arguments[i + 1].empty() || arguments[i + 1][0] != L'-')) ++i;
    }
    return std::nullopt;
}

inline Json FailureDiagnostic(const std::string& failure, int exitCode, const std::string& operationId) {
    if (!ValidOperationId(operationId)) throw std::runtime_error("E_DIAGNOSTIC_ID");
    const auto errorCode = NeutralErrorCode(failure);
    return Json{
        {"schemaVersion", 1},
        {"product", "ccode"},
        {"platform", "windows"},
        {"architecture", "x64"},
        {"status", "error"},
        {"operationId", operationId},
        {"errorCode", errorCode},
        {"category", DiagnosticCategory(errorCode)},
        {"exitCode", exitCode},
        {"privacy", {
            {"argumentsCaptured", false},
            {"environmentValuesCaptured", false},
            {"promptOrContentCaptured", false},
            {"credentialsCaptured", false},
        }},
    };
}

}  // namespace ccode
