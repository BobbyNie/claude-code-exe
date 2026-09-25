#include "../../scripts/ccode/permission.hpp"
#include <cassert>
int main() {
    const ccode::Json input = {{"file_path", "D:\\user\\claude\\a.txt"}, {"content", "hello"}};
    const ccode::Json args = {{"tool_name", "Write"}, {"input", input}};
    auto approved = ccode::PermissionDecision(args, true);
    assert(approved["behavior"] == "allow" && approved["updatedInput"] == input);
    assert(ccode::PermissionDecision(args, false)["behavior"] == "deny");
    assert(ccode::PermissionDecision({{"tool_name", ""}, {"input", input}}, true)["behavior"] == "deny");
    auto response = ccode::PermissionRpc({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}},
        [](const ccode::Json&) { return false; });
    assert(response["result"]["tools"][0]["name"] == "approve");
    auto call = ccode::PermissionRpc({{"id", 2}, {"method", "tools/call"},
        {"params", {{"name", "approve"}, {"arguments", args}}}}, [](const ccode::Json&) { return false; });
    assert(ccode::Json::parse(call["result"]["content"][0]["text"].get<std::string>())["behavior"] == "deny");
}
