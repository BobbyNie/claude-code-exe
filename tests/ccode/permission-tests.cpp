#include "../../scripts/ccode/permission.hpp"
#include <cassert>
int main() {
    // Latest engines must not silently opt into an upstream auto classifier.
    assert((ccode::PermissionArguments({}) == std::vector<std::wstring>{L"--permission-mode", L"default"}));
    const std::vector<std::wstring> allowed{L"--allowedTools", L"Read,Bash"};
    assert((ccode::PermissionArguments(allowed) == std::vector<std::wstring>{
        L"--permission-mode", L"default", L"--allowedTools", L"Read,Bash"}));
    for (const auto mode : {L"default", L"plan", L"acceptEdits"}) {
        const std::vector<std::wstring> explicitMode{L"--permission-mode", mode, L"--allowedTools", L"Read"};
        assert(ccode::PermissionArguments(explicitMode) == explicitMode);
    }
    const std::vector<std::wstring> optionLikeValue{L"--append-system-prompt", L"--permission-mode"};
    assert(ccode::PermissionArguments(optionLikeValue).size() == 4);

    const ccode::Json input = {{"file_path", "D:\\user\\claude\\a.txt"}, {"content", "hello"}};
    const ccode::Json args = {{"tool_name", "Write"}, {"input", input}};
    auto approved = ccode::PermissionDecision(args, true);
    assert(approved["behavior"] == "allow" && approved["updatedInput"] == input);
    assert(ccode::PermissionDecision(args, false)["behavior"] == "deny");
    assert(ccode::PermissionDecision({{"tool_name", ""}, {"input", input}}, true)["behavior"] == "deny");
    auto response = ccode::PermissionRpc({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}},
        [](const ccode::Json&) { return false; });
    assert(response["result"]["tools"][0]["name"] == "approve");
    auto call = ccode::PermissionRpc({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/call"},
        {"params", {{"name", "approve"}, {"arguments", args}}}}, [](const ccode::Json&) { return false; });
    assert(ccode::Json::parse(call["result"]["content"][0]["text"].get<std::string>())["behavior"] == "deny");
    const ccode::Json valid = {{"jsonrpc", "2.0"}, {"id", 7}, {"method", "tools/call"},
        {"params", {{"name", "approve"}, {"arguments", args}}}};
    for (const auto& version : {ccode::Json(nullptr), ccode::Json(2), ccode::Json("1.0")}) {
        auto invalid = valid;
        invalid["jsonrpc"] = version;
        bool asked = false;
        const auto rejected = ccode::PermissionRpc(invalid, [&](const ccode::Json&) { asked = true; return true; });
        assert(!asked && rejected["error"]["code"] == -32600);
        assert(!rejected.contains("result"));
    }
    auto missingVersion = valid;
    missingVersion.erase("jsonrpc");
    bool askedWithoutVersion = false;
    const auto rejectedVersion = ccode::PermissionRpc(missingVersion,
        [&](const ccode::Json&) { askedWithoutVersion = true; return true; });
    assert(!askedWithoutVersion && rejectedVersion["error"]["code"] == -32600);

    for (const auto& invalidId : {ccode::Json(nullptr), ccode::Json(true), ccode::Json(1.5),
                                 ccode::Json::array({1}), ccode::Json{{"private", "marker"}}}) {
        auto invalid = valid;
        invalid["id"] = invalidId;
        bool asked = false;
        const auto rejected = ccode::PermissionRpc(invalid, [&](const ccode::Json&) { asked = true; return true; });
        assert(!asked && rejected["error"]["code"] == -32600 && rejected["id"].is_null());
        assert(!rejected.contains("result"));
    }
    for (const auto& validId : {ccode::Json(7), ccode::Json("request-7")}) {
        auto request = valid;
        request["id"] = validId;
        int prompts = 0;
        const auto result = ccode::PermissionRpc(request, [&](const ccode::Json& received) {
            ++prompts; assert(received == args); return true;
        });
        assert(prompts == 1 && result["id"] == validId);
        assert(ccode::Json::parse(result["result"]["content"][0]["text"].get<std::string>())["behavior"] == "allow");
    }
    auto notification = valid;
    notification.erase("id");
    bool promptedNotification = false;
    assert(ccode::PermissionRpc(notification, [&](const ccode::Json&) {
        promptedNotification = true; return true;
    }).is_null());
    assert(!promptedNotification);

}
