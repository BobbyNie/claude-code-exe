#include "../../scripts/ccode/frontend.hpp"
#include <cassert>
#include <iostream>

void ExpectError(ccode::EventReader& reader, const std::string& wire, const std::string& code) {
    bool rejected = false;
    try { reader.Feed(wire); }
    catch (const std::runtime_error& error) { rejected = error.what() == code; }
    assert(rejected);
}

int main() {
    // Protocol errors are stable neutral codes, never parser diagnostics containing input.
    ccode::EventReader invalidJson;
    ExpectError(invalidJson, "{secret-token:bad}\n", "E_PROTOCOL_JSON");
    // Every byte boundary, including within UTF-8, must preserve a complete event.
    const std::string wire = u8"{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"text\",\"text\":\"中文🙂\"}]}}\n"
        "{\"type\":\"result\",\"subtype\":\"success\"}\n";
    for (size_t split = 0; split <= wire.size(); ++split) {
        ccode::EventReader fragmented;
        auto output = fragmented.Feed(wire.substr(0, split));
        output += fragmented.Feed(wire.substr(split));
        fragmented.Finish();
        assert(output == u8"中文🙂\n");
    }
    // Engine-generated error text is untrusted diagnostics, not model output.
    for (const auto& code : {"authentication_failed", "rate_limit", "future-private-error"}) {
        ccode::EventReader rejected;
        const auto errorEvent = ccode::Json{{"type", "assistant"}, {"error", code},
            {"message", {{"content", ccode::Json::array({
                {{"type", "text"}, {"text", "private-gateway-detail secret-token"}}
            })}}}}.dump() + "\n";
        const auto expected = std::string(code) == "authentication_failed"
            ? "[E_GATEWAY_AUTH: authentication failed]\n"
            : std::string(code) == "rate_limit" ? "[E_GATEWAY_RATE_LIMIT: request rate limited]\n"
            : "[E_ENGINE: request failed]\n";
        assert(rejected.Feed(errorEvent) == expected);
        assert(rejected.failed);
        // A contradictory success result must not erase an earlier error.
        rejected.Feed("{\"type\":\"result\",\"subtype\":\"success\"}\n");
        rejected.Finish();
        assert(rejected.failed);
    }
    ccode::EventReader badShape;
    ExpectError(badShape, "{\"type\":42}\n", "E_PROTOCOL_SCHEMA");
    // After any invalid event, later input must not revive the failed turn.
    ExpectError(invalidJson, wire, "E_PROTOCOL_FAILED");
    const auto init = ccode::Json{{"type", "system"}, {"subtype", "init"},
        {"session_id", "s1"}, {"tools", {"Read", "Write"}}}.dump() + "\n";
    auto tool = [](const std::string& name, const std::string& id, const ccode::Json& input) {
        return ccode::Json{{"type", "assistant"}, {"message", {{"content", ccode::Json::array({
            {{"type", "tool_use"}, {"name", name}, {"id", id}, {"input", input}}
        })}}}}.dump() + "\n";
    };
    for (const auto& test : std::vector<std::pair<std::string, std::string>>{
        {tool("", "t1", ccode::Json::object()), "E_TOOL_NAME"},
        {tool("Read", "", ccode::Json::object()), "E_TOOL_ID"},
        {tool("Read", "t1", "not-an-object"), "E_TOOL_INPUT"},
        {tool("NotRegistered", "t1", ccode::Json::object()), "E_TOOL_UNKNOWN"}
    }) {
        ccode::EventReader invalidTool;
        invalidTool.Feed(init);
        ExpectError(invalidTool, test.first, test.second);
    }
    ccode::EventReader duplicate;
    duplicate.Feed(init);
    const auto read = tool("Read", "t1", {{"file_path", "user/claude-original.txt"}});
    assert(duplicate.Feed(read) == "[Tool request]\n");
    ExpectError(duplicate, read, "E_TOOL_DUPLICATE_ID");
    // Tool argument fragments are transport bytes, not separate tool invocations.
    for (size_t split = 0; split <= read.size(); ++split) {
        ccode::EventReader fragmented;
        fragmented.Feed(init);
        auto output = fragmented.Feed(read.substr(0, split));
        output += fragmented.Feed(read.substr(split));
        assert(output == "[Tool request]\n");
    }
    ccode::EventReader invalidUtf8;
    ExpectError(invalidUtf8, std::string("{\"type\":\"") + char(0xff) + "\"}\n", "E_PROTOCOL_JSON");
    // The limit applies to one JSON line, independent of transport chunking.
    const size_t eventLimit = 16 * 1024 * 1024;
    const std::string paddingPrefix = "{\"type\":\"system\",\"padding\":\"";
    const std::string paddingSuffix = "\"}";
    const auto atLimit = paddingPrefix +
        std::string(eventLimit - paddingPrefix.size() - paddingSuffix.size(), 'x') + paddingSuffix;
    const std::string resultLine = "{\"type\":\"result\",\"subtype\":\"success\"}\n";
    for (bool fragmented : {false, true}) {
        ccode::EventReader boundary;
        if (fragmented) {
            assert(boundary.Feed(atLimit).empty());
            assert(boundary.Feed("\n" + atLimit + "\n" + resultLine).empty());
        } else {
            assert(boundary.Feed(atLimit + "\n" + atLimit + "\n" + resultLine).empty());
        }
        boundary.Finish();
        assert(boundary.complete && !boundary.failed);
    }
    ccode::EventReader oversized;
    ExpectError(oversized, std::string(16 * 1024 * 1024 + 1, 'x'), "E_EVENT_LIMIT");
    ExpectError(oversized, resultLine, "E_PROTOCOL_FAILED");
    ccode::EventReader splitOversized;
    assert(splitOversized.Feed(atLimit).empty());
    ExpectError(splitOversized, " \n", "E_EVENT_LIMIT");
    ExpectError(splitOversized, resultLine, "E_PROTOCOL_FAILED");
    for (const auto& entry : std::vector<std::pair<std::string, std::string>>{
        {"", "E_MISSING_RESULT"},
        {"{\"type\":\"result\"", "E_TRUNCATED_EVENT"}
    }) {
        ccode::EventReader incomplete;
        incomplete.Feed(entry.first);
        bool rejected = false;
        try { incomplete.Finish(); }
        catch (const ccode::ProtocolError& error) { rejected = error.what() == entry.second; }
        assert(rejected);
        ExpectError(incomplete, wire, "E_PROTOCOL_FAILED");
    }
    ccode::EventReader reader;
    auto start = reader.Feed(init);
    assert(start.empty());
    assert(reader.session == "s1");
    assert(reader.Feed("{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"text\",\"text\":\"hello").empty());
    auto text = reader.Feed(" world\"}]}}\n");
    assert(text == "hello world\n");
    reader.Feed("{\"type\":\"result\",\"subtype\":\"success\",\"is_error\":false,\"session_id\":\"s1\"}\n");
    reader.Finish();
    assert(reader.complete && !reader.failed);
    for (const auto& invalid : {
        "{bad}\n",
        "{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"tool_use\",\"name\":\"\",\"id\":\"t1\",\"input\":{}}]}}\n",
        "{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"tool_use\",\"name\":\"Read\",\"id\":\"t1\",\"input\":\"bad\"}]}}\n"
    }) {
        bool rejected = false;
        try { ccode::EventReader broken; broken.Feed(invalid); }
        catch (const std::exception&) { rejected = true; }
        assert(rejected);
    }
    assert(ccode::ConsoleText("\x1b[2Jhello\r") == "[2Jhello");
    // A complete event must not be silently accepted after the terminal result.
    bool afterResult = false;
    try { reader.Feed("{\"type\":\"assistant\",\"message\":{\"content\":[]}}\n"); }
    catch (...) { afterResult = true; }
    assert(afterResult);
    std::cout << "frontend event tests passed\n";
}
