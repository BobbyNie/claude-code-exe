#pragma once
#include "vendor/json.hpp"
#include <set>
#include <stdexcept>
#include <string>

namespace ccode {
using Json = nlohmann::json;
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
    std::set<std::string> toolIds;
    std::string Event(const Json& event) {
        if (!event.is_object()) throw std::runtime_error("E_PROTOCOL");
        const auto type = event.value("type", std::string());
        if (complete && (type == "assistant" || type == "result")) throw std::runtime_error("E_PROTOCOL_ORDER");
        if (event.contains("session_id")) session = event.at("session_id").get<std::string>();
        if (type == "assistant") {
            std::string output;
            const auto& content = event.at("message").at("content");
            if (!content.is_array()) throw std::runtime_error("E_PROTOCOL");
            for (const auto& block : content) {
                auto kind = block.value("type", std::string());
                if (kind == "text") output += ConsoleText(block.at("text").get<std::string>()) + "\n";
                if (kind == "tool_use") {
                    const auto name = block.at("name").get<std::string>();
                    const auto id = block.at("id").get<std::string>();
                    if (name.empty() || id.empty() || !block.at("input").is_object() ||
                        !toolIds.insert(id).second) throw std::runtime_error("E_TOOL_PROTOCOL");
                    // Do not expose internal tool names or paths as product chrome.
                    output += "[Tool request]\n";
                }
            }
            return output;
        }
        if (type == "result") {
            if (complete) throw std::runtime_error("E_PROTOCOL");
            complete = true;
            failed = event.value("is_error", false) || event.value("subtype", std::string()) != "success";
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
        pending += bytes;
        if (pending.size() > 16 * 1024 * 1024) throw std::runtime_error("E_EVENT_LIMIT");
        std::string output;
        size_t end;
        while ((end = pending.find('\n')) != std::string::npos) {
            auto line = pending.substr(0, end);
            pending.erase(0, end + 1);
            if (!line.empty() && line != "\r") output += Event(Json::parse(line));
        }
        return output;
    }
    void Finish() {
        if (!pending.empty()) {
            if (pending != "\r") throw std::runtime_error("E_TRUNCATED_EVENT");
        }
        if (!complete) throw std::runtime_error("E_MISSING_RESULT");
    }
};
}
