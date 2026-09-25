#include "../../scripts/ccode/sessions.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
int main() {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / ("ccode-sessions-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    auto project = root / "projects" / "old-key";
    fs::create_directories(project);
    const std::string id = "12345678-1234-1234-1234-123456789abc";
    { std::ofstream out(project / (id + ".jsonl"));
      out << ccode::Json{{"type", "user"}, {"sessionId", id}, {"cwd", root.u8string()},
        {"isSidechain", false}, {"message", {{"role", "user"}, {"content", "hello history"}}}}.dump() << '\n';
      out << "{truncated"; }
    auto sessions = ccode::ListSessions(root / "projects", root);
    assert(sessions.size() == 1 && sessions[0].id == id);
    assert(sessions[0].title == "hello history");
    assert(ccode::ListSessions(root / "projects", root / "other").empty());
    assert(!ccode::ValidSessionId("../../escape"));
    assert(!ccode::ValidSessionId("123"));
    fs::remove_all(root);
    std::cout << "session discovery passed\n";
}
