#pragma once
#include "frontend.hpp"
#include <functional>

namespace ccode {
inline Json PermissionDecision(const Json& args, bool allow) {
    if (!args.is_object() || !args.contains("tool_name") || !args["tool_name"].is_string() ||
        args["tool_name"].get<std::string>().empty() || !args.contains("input") || !args["input"].is_object())
        return {{"behavior", "deny"}, {"message", "Invalid permission request"}};
    if (allow) return {{"behavior", "allow"}, {"updatedInput", args["input"]}};
    return {{"behavior", "deny"}, {"message", "User did not approve this operation"}};
}
inline Json PermissionRpc(const Json& request, const std::function<bool(const Json&)>& approve) {
    if (!request.is_object() || !request.contains("id")) return nullptr;
    Json response = {{"jsonrpc", "2.0"}, {"id", request["id"]}};
    const auto method = request.value("method", std::string());
    if (method == "initialize") {
        response["result"] = {{"protocolVersion", "2024-11-05"},
            {"capabilities", {{"tools", Json::object()}}},
            {"serverInfo", {{"name", "ccode-permissions"}, {"version", "1.0.0"}}}};
    } else if (method == "ping") response["result"] = Json::object();
    else if (method == "tools/list") {
        response["result"] = {{"tools", Json::array({{
            {"name", "approve"}, {"description", "Ask the local user to approve a tool operation"},
            {"inputSchema", {{"type", "object"}, {"required", {"tool_name", "input"}},
                {"properties", {{"tool_name", {{"type", "string"}}},
                    {"input", {{"type", "object"}, {"additionalProperties", true}}},
                    {"tool_use_id", {{"type", "string"}}}}}}}
        }})}};
    } else if (method == "tools/call" && request.at("params").value("name", std::string()) == "approve") {
        const auto& args = request.at("params").at("arguments");
        auto decision = PermissionDecision(args, false);
        if (args.is_object() && args.contains("input") && args["input"].is_object() &&
            args.contains("tool_name") && args["tool_name"].is_string() && !args["tool_name"].get<std::string>().empty())
            decision = PermissionDecision(args, approve(args));
        response["result"] = {{"content", Json::array({{{"type", "text"}, {"text", decision.dump()}}})}};
    } else response["error"] = {{"code", -32601}, {"message", "Unsupported method"}};
    return response;
}
}
