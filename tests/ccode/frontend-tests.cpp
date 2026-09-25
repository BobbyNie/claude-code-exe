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
    ccode::EventReader oversized;
    ExpectError(oversized, std::string(16 * 1024 * 1024 + 1, 'x'), "E_EVENT_LIMIT");
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
