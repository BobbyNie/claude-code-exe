#pragma once
#include "vendor/json.hpp"
#include <set>
#include <stdexcept>
#include <string>

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
    std::string Event(const Json& event) {
        if (!event.is_object()) throw ProtocolError("E_PROTOCOL");
        const auto type = event.value("type", std::string());
        if (complete && (type == "assistant" || type == "result")) throw ProtocolError("E_PROTOCOL_ORDER");
        if (event.contains("session_id")) session = event.at("session_id").get<std::string>();
        if (type == "system" && event.value("subtype", std::string()) == "init") {
            const auto& tools = event.at("tools");
            if (!tools.is_array()) throw ProtocolError("E_PROTOCOL_SCHEMA");
            registeredTools.clear();
            for (const auto& tool : tools) {
                const auto name = tool.get<std::string>();
                if (name.empty()) throw ProtocolError("E_TOOL_NAME");
                registeredTools.insert(name);
            }
            toolRegistryReceived = true;
            return "";
        }
        if (type == "assistant") {
            // SDK assistant errors can embed raw gateway bodies in text blocks.
            // Classify only the structured field; never display those blocks.
            if (event.contains("error") && !event.at("error").is_null()) {
                const auto error = event.at("error").get<std::string>();
                failed = true;
                return error == "authentication_failed"
                    ? "[E_GATEWAY_AUTH: authentication failed]\n"
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
                    // Do not expose internal tool names or paths as product chrome.
                    output += "[Tool request]\n";
                }
            }
            return output;
        }
        if (type == "result") {
            if (complete) throw ProtocolError("E_PROTOCOL");
            complete = true;
            failed = failed || event.value("is_error", false) || event.value("subtype", std::string()) != "success";
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
    bool complete = false, failed = false;
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
                    auto event = Json::parse(pending, nullptr, false);
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
