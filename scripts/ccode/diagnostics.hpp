#pragma once
#include "frontend.hpp"
#include <cctype>
#include <filesystem>
#include <optional>
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
    return message.substr(0, end);
}

inline std::string DiagnosticCategory(const std::string& errorCode) {
    if (HasPrefix(errorCode, "E_GATEWAY") || HasPrefix(errorCode, "E_NETWORK") ||
        HasPrefix(errorCode, "E_TLS") || HasPrefix(errorCode, "E_DNS")) return "network";
    if (HasPrefix(errorCode, "E_PROTOCOL") || HasPrefix(errorCode, "E_EVENT") ||
        HasPrefix(errorCode, "E_TOOL")) return "protocol";
    if (errorCode.find("_BUSY") != std::string::npos || HasPrefix(errorCode, "E_SESSION_LOCK"))
        return "concurrency";
    if (HasPrefix(errorCode, "E_CREDENTIAL") || HasPrefix(errorCode, "E_ARGUMENT") ||
        HasPrefix(errorCode, "E_UNSUPPORTED_OPTION") || HasPrefix(errorCode, "E_PERMISSION_MODE") ||
        HasPrefix(errorCode, "E_API_ONLY") || HasPrefix(errorCode, "E_PROMPT")) return "configuration";
    if (HasPrefix(errorCode, "E_PACKAGE") || HasPrefix(errorCode, "E_RESOURCE") ||
        HasPrefix(errorCode, "E_HASH") || HasPrefix(errorCode, "E_RUNTIME") ||
        HasPrefix(errorCode, "E_CANDIDATE") || HasPrefix(errorCode, "E_SNAPSHOT") ||
        HasPrefix(errorCode, "E_ACTIVATION") || HasPrefix(errorCode, "E_ROLLBACK")) return "integrity";
    if (HasPrefix(errorCode, "E_PROFILE") || HasPrefix(errorCode, "E_SESSION") ||
        HasPrefix(errorCode, "E_WORKSPACE") || HasPrefix(errorCode, "E_DATA") ||
        HasPrefix(errorCode, "E_NO_SESSION")) return "data";
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
