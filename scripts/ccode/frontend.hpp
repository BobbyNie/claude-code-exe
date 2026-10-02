#pragma once
#include "vendor/json.hpp"
#include <set>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ccode {
using Json = nlohmann::json;
class ProtocolError : public std::runtime_error {
public:
    explicit ProtocolError(const char* code) : std::runtime_error(code) {}
};
inline std::string ConsoleText(const std::string& text) {
    std::string result;
    for (unsigned char ch : text)
        if (ch == '\n' || ch == '\t' || (ch >= 32 && ch != 127)) result += ch;
    return result;
}

// The engine owns tool execution. This adapter validates complete output events;
// it never repairs or executes a model's incomplete tool call.
class EventReader {
    std::string pending;
    bool broken = false;
    std::set<std::string> toolIds;
    std::set<std::string> registeredTools;
    bool toolRegistryReceived = false;
    std::set<std::string> subagentToolIds;
    std::map<std::string, std::string> backgroundTasks;
    std::set<std::string> seenTaskIds;
    bool interimResult = false;
    size_t queuedParentTurns = 0;
    std::string Event(const Json& event) {
        if (!event.is_object()) throw ProtocolError("E_PROTOCOL");
        const auto type = event.value("type", std::string());
        if (complete && (type == "assistant" || type == "result")) throw ProtocolError("E_PROTOCOL_ORDER");
        // Native2.1.282's typed API cause comes from the local error adapter.
        // Both fields must be top-level wrapper siblings; never inspect content.
        if (type == "assistant" && event.contains("is_api_error_message") &&
            event.at("is_api_error_message").is_boolean() &&
            event.at("is_api_error_message").get<bool>() &&
            event.contains("api_error") && event.at("api_error").is_string() &&
            event.at("api_error").get<std::string>() == "tls_untrusted_ca")
            throw ProtocolError("E_GATEWAY_TLS");
        // Only structured engine codes are diagnostic evidence. Model text,
        // arbitrary error messages and transport resets cannot establish TLS trust failure.
        if ((type == "assistant" || type == "system" || type == "result") &&
            event.contains("error") && event.at("error").is_object()) {
            const auto& error = event.at("error");
            if (error.contains("code") && error.at("code").is_string()) {
                const auto code = error.at("code").get<std::string>();
                if (code == "ENOTFOUND" || code == "EAI_AGAIN")
                    throw ProtocolError("E_GATEWAY_DNS");
                if (code == "CERT_HAS_EXPIRED" || code == "DEPTH_ZERO_SELF_SIGNED_CERT" ||
                    code == "SELF_SIGNED_CERT_IN_CHAIN" || code == "UNABLE_TO_VERIFY_LEAF_SIGNATURE" ||
                    code == "UNABLE_TO_GET_ISSUER_CERT_LOCALLY" || code == "ERR_TLS_CERT_ALTNAME_INVALID")
                    throw ProtocolError("E_GATEWAY_TLS");
            }
        }
        // The engine can have cause-specific retry budgets outside MAX_RETRIES.
        // Stop on its documented pre-retry event; RunTurn terminates the Job Object.
        // Do not render error fields or rely on a particular delay/category.
        if (type == "system" && event.value("subtype", std::string()) == "api_retry")
            throw ProtocolError("E_GATEWAY_RETRY");
        // Initialization fixes the registry for this turn. Reject a replacement
        // before it can mutate the session identity or registered tool names.
        if (type == "system" && event.value("subtype", std::string()) == "init" &&
            (toolRegistryReceived || complete)) {
            // Native background completion queues a fresh parent turn: result,
            // init, assistant, result. Only a matched completed task can grant
            // this boundary, never an arbitrary duplicate initialization.
            if (!complete || !queuedParentTurns || !backgroundTasks.empty())
                throw ProtocolError("E_PROTOCOL_ORDER");
            --queuedParentTurns;
            complete = false;
            interimResult = false;
            toolIds.clear();
            subagentToolIds.clear();
        }
        if (event.contains("session_id")) {
            const auto eventSession = event.at("session_id").get<std::string>();
            if (!session.empty() && eventSession != session) throw ProtocolError("E_SESSION_MISMATCH");
            session = eventSession;
        }
        if (type == "system" && event.value("subtype", std::string()) == "init") {
            const auto& tools = event.at("tools");
            if (!tools.is_array()) throw ProtocolError("E_PROTOCOL_SCHEMA");
            registeredTools.clear();
            for (const auto& tool : tools) {
                const auto name = tool.get<std::string>();
                if (name.empty()) throw ProtocolError("E_TOOL_NAME");
                if (!registeredTools.insert(name).second) throw ProtocolError("E_TOOL_DUPLICATE_NAME");
            }
            // Observed native 2.1.282 protocol: init lists Task, while the
            // successful subagent event uses Agent. This is a one-way alias,
            // not permission to accept tools absent from the declared registry.
            if (registeredTools.count("Task")) registeredTools.insert("Agent");
            toolRegistryReceived = true;
            return "";
        }
        if (type == "system" && event.value("subtype", std::string()) == "task_started") {
            const auto task = event.at("task_id").get<std::string>();
            const auto tool = event.at("tool_use_id").get<std::string>();
            if (complete || task.empty() || !subagentToolIds.count(tool) ||
                !seenTaskIds.insert(task).second || !backgroundTasks.emplace(task, tool).second) throw ProtocolError("E_PROTOCOL_ORDER");
            return "";
        }
        if (type == "system" && event.value("subtype", std::string()) == "task_notification") {
            const auto task = event.at("task_id").get<std::string>();
            const auto status = event.at("status").get<std::string>();
            if (complete || !backgroundTasks.count(task) ||
                (status != "completed" && status != "failed" && status != "stopped"))
                throw ProtocolError("E_PROTOCOL_ORDER");
            backgroundTasks.erase(task);
            if (status == "completed" && !interimResult) ++queuedParentTurns;
            if (status != "completed") { failed = true; failureCode = "E_ENGINE"; }
            // Completion notification authorizes a subsequent parent model turn;
            // a second result without this boundary is still invalid.
            interimResult = false;
            return "";
        }
        if (type == "assistant") {
            // SDK assistant errors can embed raw gateway bodies in text blocks.
            // Classify only the structured field; never display those blocks.
            if (event.contains("error") && !event.at("error").is_null()) {
                const auto error = event.at("error").get<std::string>();
                failed = true;
                if (failureCode.empty()) failureCode = error == "authentication_failed"
                    ? "E_GATEWAY_AUTH" : error == "rate_limit" ? "E_GATEWAY_RATE_LIMIT" : "E_ENGINE";
                return error == "authentication_failed"
                    ? "[E_GATEWAY_AUTH: authentication failed]\n"
                    : error == "rate_limit" ? "[E_GATEWAY_RATE_LIMIT: request rate limited]\n"
                    : "[E_ENGINE: request failed]\n";
            }
            std::string output;
            const auto& content = event.at("message").at("content");
            if (!content.is_array()) throw ProtocolError("E_PROTOCOL");
            for (const auto& block : content) {
                auto kind = block.value("type", std::string());
                if (kind == "text") output += ConsoleText(block.at("text").get<std::string>()) + "\n";
                if (kind == "tool_use") {
                    const auto name = block.at("name").get<std::string>();
                    const auto id = block.at("id").get<std::string>();
                    if (name.empty()) throw ProtocolError("E_TOOL_NAME");
                    if (id.empty()) throw ProtocolError("E_TOOL_ID");
                    if (!block.at("input").is_object()) throw ProtocolError("E_TOOL_INPUT");
                    if (!toolRegistryReceived) throw ProtocolError("E_PROTOCOL_ORDER");
                    if (!registeredTools.count(name)) throw ProtocolError("E_TOOL_UNKNOWN");
                    if (!toolIds.insert(id).second) throw ProtocolError("E_TOOL_DUPLICATE_ID");
                    if (name == "Agent" || name == "Task") subagentToolIds.insert(id);
                    // Do not expose internal tool names or paths as product chrome.
                    output += "[Tool request]\n";
                }
            }
            return output;
        }
        if (type == "result") {
            if (complete || interimResult) throw ProtocolError("E_PROTOCOL");
            complete = backgroundTasks.empty();
            interimResult = !complete;
            failed = failed || event.value("is_error", false) || event.value("subtype", std::string()) != "success";
            if (failed && failureCode.empty()) failureCode = "E_ENGINE";
            if (event.contains("permission_denials") && !event["permission_denials"].empty())
                return "[Some tool requests were denied]\n";
            return failed ? "[E_ENGINE: turn failed]\n" : "";
        }
        if (type == "system" || type == "user" || type == "stream_event" ||
            type == "tool_progress" || type == "tool_use_summary" || type == "rate_limit_event") return "";
        // Unknown engine events are not raw terminal output.
        return "";
    }
public:
    std::string session;
    std::string failureCode;
    bool complete = false, failed = false;
    explicit EventReader(const std::string& expectedSession = "") : session(expectedSession) {}
    std::string Feed(const std::string& bytes) {
        if (broken) throw ProtocolError("E_PROTOCOL_FAILED");
        try {
            constexpr size_t eventLimit = 16 * 1024 * 1024;
            std::string output;
            size_t offset = 0;
            while (offset < bytes.size()) {
                const auto end = bytes.find('\n', offset);
                const auto count = (end == std::string::npos ? bytes.size() : end) - offset;
                // Bound each line before allocating; a read may contain many legal events.
                if (count > eventLimit - pending.size()) throw ProtocolError("E_EVENT_LIMIT");
                pending.append(bytes, offset, count);
                if (end == std::string::npos) break;
                if (!pending.empty() && pending != "\r") {
                    // Detect decoded duplicate keys before Event can update turn
                    // state. Each object has its own scope, including array items.
                    std::vector<std::set<std::string>> objectKeys;
                    const auto uniqueKeys = [&](int, Json::parse_event_t kind, Json& value) {
                        if (kind == Json::parse_event_t::object_start) objectKeys.emplace_back();
                        else if (kind == Json::parse_event_t::object_end) objectKeys.pop_back();
                        else if (kind == Json::parse_event_t::key &&
                            !objectKeys.back().insert(value.get<std::string>()).second)
                            throw ProtocolError("E_PROTOCOL_DUPLICATE_KEY");
                        return true;
                    };
                    auto event = Json::parse(pending, uniqueKeys, false);
                    if (event.is_discarded()) throw ProtocolError("E_PROTOCOL_JSON");
                    output += Event(event);
                }
                pending.clear();
                offset = end + 1;
            }
            return output;
        } catch (const Json::exception&) {
            broken = true;
            throw ProtocolError("E_PROTOCOL_SCHEMA");
        } catch (...) {
            broken = true;
            throw;
        }
    }
    void Finish() {
        if (broken) throw ProtocolError("E_PROTOCOL_FAILED");
        if (!pending.empty() && pending != "\r") {
            broken = true;
            throw ProtocolError("E_TRUNCATED_EVENT");
        }
        if (!complete) {
            broken = true;
            throw ProtocolError("E_MISSING_RESULT");
        }
    }
};
}
