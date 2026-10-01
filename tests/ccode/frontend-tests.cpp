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
    // A last-key-wins parser must not turn a failed result into success.
    const std::string ambiguousResult =
        "{\"type\":\"result\",\"subtype\":\"success\",\"is_error\":true,\"is_error\":false}\n";
    for (size_t split = 0; split < ambiguousResult.size(); ++split) {
        ccode::EventReader ambiguous;
        assert(ambiguous.Feed(ambiguousResult.substr(0, split)).empty());
        ExpectError(ambiguous, ambiguousResult.substr(split), "E_PROTOCOL_DUPLICATE_KEY");
        assert(!ambiguous.complete);
        ExpectError(ambiguous, "{\"type\":\"result\",\"subtype\":\"success\"}\n", "E_PROTOCOL_FAILED");
    }
    // Nested and escaped spellings are the same decoded key, not distinct fields.
    for (const std::string event : {
        R"({"type":"assistant","message":{"content":[{"type":"text","text":"hidden","text":"private-output"}]}})",
        R"({"type":"assistant","message":{"content":[{"type":"text","text":"hidden","\u0074ext":"private-output"}]}})",
        R"({"type":"system","subtype":"init","session_id":"old","session_id":"new","tools":[]})"}) {
        const auto line = event + "\n";
        for (size_t split = 0; split < line.size(); ++split) {
            ccode::EventReader nested("expected-session");
            assert(nested.Feed(line.substr(0, split)).empty());
            ExpectError(nested, line.substr(split), "E_PROTOCOL_DUPLICATE_KEY");
            assert(nested.session == "expected-session" && !nested.complete);
        }
    }
    // Key reuse in sibling objects and later events is legal.
    ccode::EventReader scopedKeys;
    const auto siblings = std::string(R"({"type":"assistant","message":{"content":[{"type":"text","text":"one"},{"type":"text","text":"two"}]}})") + "\n";
    assert(scopedKeys.Feed(siblings + siblings) == "one\ntwo\none\ntwo\n");
    scopedKeys.Feed("{\"type\":\"result\",\"subtype\":\"success\"}\n");
    scopedKeys.Finish();
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
        const auto expectedCode = std::string(code) == "authentication_failed"
            ? "E_GATEWAY_AUTH" : std::string(code) == "rate_limit"
            ? "E_GATEWAY_RATE_LIMIT" : "E_ENGINE";
        assert(rejected.failureCode == expectedCode);
        // A contradictory success result must not erase an earlier error.
        rejected.Feed("{\"type\":\"result\",\"subtype\":\"success\"}\n");
        rejected.Finish();
        assert(rejected.failed);
        assert(rejected.failureCode == expectedCode);
    }
    // A documented retry notification is a stop boundary, not progress chrome.
    // Reject every transport split without exposing untrusted retry details.
    const auto retry = ccode::Json{{"type", "system"}, {"subtype", "api_retry"},
        {"attempt", 1}, {"max_retries", 1}, {"retry_delay_ms", 1000},
        {"error_status", nullptr}, {"error", "private-gateway-detail secret-token"}}.dump() + "\n";
    for (size_t split = 0; split < retry.size(); ++split) {
        ccode::EventReader retrying;
        assert(retrying.Feed(retry.substr(0, split)).empty());
        ExpectError(retrying, retry.substr(split), "E_GATEWAY_RETRY");
        ExpectError(retrying, wire, "E_PROTOCOL_FAILED");
    }
    ccode::EventReader batchedRetry;
    ExpectError(batchedRetry, retry + wire, "E_GATEWAY_RETRY");
    ExpectError(batchedRetry, wire, "E_PROTOCOL_FAILED");
    ccode::EventReader badShape;
    ExpectError(badShape, "{\"type\":42}\n", "E_PROTOCOL_SCHEMA");
    // After any invalid event, later input must not revive the failed turn.
    ExpectError(invalidJson, wire, "E_PROTOCOL_FAILED");
    const auto init = ccode::Json{{"type", "system"}, {"subtype", "init"},
        {"session_id", "s1"}, {"tools", {"Read", "Write"}}}.dump() + "\n";
    const auto duplicateDeclaration = ccode::Json{{"type", "system"}, {"subtype", "init"},
        {"tools", {"Read", "Read"}}}.dump() + "\n";
    for (size_t split = 0; split < duplicateDeclaration.size(); ++split) {
        ccode::EventReader invalidRegistry;
        assert(invalidRegistry.Feed(duplicateDeclaration.substr(0, split)).empty());
        ExpectError(invalidRegistry, duplicateDeclaration.substr(split), "E_TOOL_DUPLICATE_NAME");
        ExpectError(invalidRegistry, init, "E_PROTOCOL_FAILED");
    }
    // A turn's declared tool registry cannot be replaced by another init.
    const auto replacementInit = ccode::Json{{"type", "system"}, {"subtype", "init"},
        {"session_id", "s2"}, {"tools", {"Bash"}}}.dump() + "\n";
    for (const auto& secondInit : {init, replacementInit}) {
        for (size_t split = 0; split < secondInit.size(); ++split) {
            ccode::EventReader repeatedInit;
            repeatedInit.Feed(init);
            assert(repeatedInit.Feed(secondInit.substr(0, split)).empty());
            ExpectError(repeatedInit, secondInit.substr(split), "E_PROTOCOL_ORDER");
            assert(repeatedInit.session == "s1");
            ExpectError(repeatedInit, wire, "E_PROTOCOL_FAILED");
        }
    }
    ccode::EventReader completedBeforeInit;
    completedBeforeInit.Feed("{\"type\":\"result\",\"subtype\":\"success\"}\n");
    ExpectError(completedBeforeInit, init, "E_PROTOCOL_ORDER");
    ExpectError(completedBeforeInit, wire, "E_PROTOCOL_FAILED");
    // Once an engine identifies the turn, later events cannot redirect resume.
    const auto changedSession = ccode::Json{{"type", "result"}, {"subtype", "success"},
        {"session_id", "s2"}}.dump() + "\n";
    for (size_t split = 0; split < changedSession.size(); ++split) {
        ccode::EventReader identity;
        identity.Feed(init);
        assert(identity.Feed(changedSession.substr(0, split)).empty());
        ExpectError(identity, changedSession.substr(split), "E_SESSION_MISMATCH");
        assert(identity.session == "s1" && !identity.complete);
        ExpectError(identity, wire, "E_PROTOCOL_FAILED");
    }
    // A resumed turn is pinned before the very first engine event.
    ccode::EventReader resumedIdentity("expected-session");
    ExpectError(resumedIdentity, init, "E_SESSION_MISMATCH");
    assert(resumedIdentity.session == "expected-session");
    ExpectError(resumedIdentity, wire, "E_PROTOCOL_FAILED");
    ccode::EventReader matchingResume("s1");
    matchingResume.Feed(init);
    matchingResume.Feed("{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"s1\"}\n");
    matchingResume.Finish();
    ccode::EventReader stableIdentity;
    stableIdentity.Feed(init);
    stableIdentity.Feed("{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"s1\"}\n");
    stableIdentity.Finish();
    assert(stableIdentity.session == "s1" && !stableIdentity.failed);
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
    // Native 2.1.282 declares Task but emits Agent for a successful subagent.
    ccode::EventReader subagentAlias;
    subagentAlias.Feed(ccode::Json{{"type", "system"}, {"subtype", "init"},
        {"tools", {"Task"}}}.dump() + "\n");
    assert(subagentAlias.Feed(tool("Agent", "child1", {{"prompt", "probe"}})) == "[Tool request]\n");
    ccode::EventReader undeclaredSubagent;
    undeclaredSubagent.Feed(init);
    ExpectError(undeclaredSubagent, tool("Agent", "child1", ccode::Json::object()), "E_TOOL_UNKNOWN");
    ccode::EventReader backgroundAgent;
    backgroundAgent.Feed(ccode::Json{{"type", "system"}, {"subtype", "init"},
        {"tools", {"Task"}}}.dump() + "\n");
    backgroundAgent.Feed(tool("Agent", "bg1", ccode::Json::object()));
    backgroundAgent.Feed(ccode::Json{{"type", "system"}, {"subtype", "task_started"},
        {"task_id", "task1"}, {"tool_use_id", "bg1"}}.dump() + "\n");
    const auto successResult = ccode::Json{{"type", "result"}, {"subtype", "success"},
        {"is_error", false}}.dump() + "\n";
    backgroundAgent.Feed(successResult);
    assert(!backgroundAgent.complete);
    backgroundAgent.Feed(ccode::Json{{"type", "system"}, {"subtype", "task_notification"},
        {"task_id", "task1"}, {"status", "completed"}}.dump() + "\n");
    backgroundAgent.Feed(ccode::Json{{"type", "assistant"}, {"message", {{"content",
        ccode::Json::array({{{"type", "text"}, {"text", "parent-after-child"}}})}}}}.dump() + "\n");
    backgroundAgent.Feed(successResult);
    backgroundAgent.Finish();
    assert(backgroundAgent.complete && !backgroundAgent.failed);
    // Actual native background sequence completes the task before result, then
    // starts a new parent turn with init in the same stream/session.
    ccode::EventReader queuedParent;
    const auto agentInit = ccode::Json{{"type", "system"}, {"subtype", "init"},
        {"session_id", "same"}, {"tools", {"Task"}}}.dump() + "\n";
    queuedParent.Feed(agentInit);
    queuedParent.Feed(tool("Agent", "bg2", ccode::Json::object()));
    queuedParent.Feed(ccode::Json{{"type", "system"}, {"subtype", "task_started"},
        {"task_id", "task2"}, {"tool_use_id", "bg2"}}.dump() + "\n");
    queuedParent.Feed(ccode::Json{{"type", "system"}, {"subtype", "task_notification"},
        {"task_id", "task2"}, {"status", "completed"}}.dump() + "\n");
    queuedParent.Feed(successResult);
    queuedParent.Feed(agentInit);
    queuedParent.Feed(successResult);
    queuedParent.Finish();
    assert(queuedParent.complete && queuedParent.session == "same");
    ExpectError(queuedParent, agentInit, "E_PROTOCOL_ORDER");
    ccode::EventReader reusedTask;
    reusedTask.Feed(agentInit);
    reusedTask.Feed(tool("Agent", "reuse1", ccode::Json::object()));
    const auto taskStart = ccode::Json{{"type", "system"}, {"subtype", "task_started"},
        {"task_id", "once-only"}, {"tool_use_id", "reuse1"}}.dump() + "\n";
    reusedTask.Feed(taskStart);
    reusedTask.Feed(ccode::Json{{"type", "system"}, {"subtype", "task_notification"},
        {"task_id", "once-only"}, {"status", "completed"}}.dump() + "\n");
    ExpectError(reusedTask, taskStart, "E_PROTOCOL_ORDER");
    // A notification without a registered task cannot reopen a completed turn.
    ccode::EventReader unsolicitedTask;
    unsolicitedTask.Feed(init);
    ExpectError(unsolicitedTask, ccode::Json{{"type", "system"}, {"subtype", "task_notification"},
        {"task_id", "unknown"}, {"status", "completed"}}.dump() + "\n", "E_PROTOCOL_ORDER");
    ccode::EventReader unboundTask;
    unboundTask.Feed(init);
    ExpectError(unboundTask, ccode::Json{{"type", "system"}, {"subtype", "task_started"},
        {"task_id", "unknown"}, {"tool_use_id", "unknown"}}.dump() + "\n", "E_PROTOCOL_ORDER");
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
