#include "../../scripts/ccode/sessions.hpp"
#include "../../scripts/ccode/workspaces.hpp"
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
    // Corrupt identity metadata must fail with a neutral, stable classification,
    // never disappear as an empty history or expose a JSON-library exception.
    const auto transcript = project / (id + ".jsonl");
    const ccode::Json validEvent = {{"type", "user"}, {"sessionId", id},
        {"cwd", root.u8string()}, {"isSidechain", false},
        {"message", {{"content", "private-history-marker"}}}};
    for (const auto& field : {"type", "sessionId", "cwd", "isSidechain"}) {
        auto event = validEvent;
        event[field] = 42;
        const auto bytes = event.dump() + "\n";
        { std::ofstream out(transcript, std::ios::binary); out << bytes; }
        bool classified = false;
        try { ccode::ListSessions(root / "projects", root); }
        catch (const std::exception& error) {
            classified = std::string(error.what()) == "E_SESSION_DATA";
        }
        assert(classified);
        std::ifstream input(transcript, std::ios::binary);
        assert(std::string(std::istreambuf_iterator<char>(input), {}) == bytes);
    }
    const auto registry = root / "profile" / "workspaces.json";
    auto first = ccode::ResolveWorkspace(registry, root);
    assert(ccode::ValidSessionId(first));
    assert(ccode::ResolveWorkspace(registry, root / ".") == first);
    fs::create_directories(root / "other");
    auto other = ccode::ResolveWorkspace(registry, root / "other");
    assert(other != first && ccode::ValidSessionId(other));
    assert(ccode::ResolveWorkspace(registry, root) == first);
    auto readAll = [](const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    const auto saved = readAll(registry);
    { std::ofstream out(registry); out << "{broken"; }
    bool corrupt = false;
    try { ccode::ResolveWorkspace(registry, root); }
    catch (const std::runtime_error& error) { corrupt = std::string(error.what()) == "E_WORKSPACE_DATA"; }
    assert(corrupt && readAll(registry) == "{broken");
    { std::ofstream out(registry, std::ios::binary); out << saved; }
    assert(readAll(registry) == saved);
    fs::create_directories(root / "third");
    auto candidate = registry; candidate += ".new";
    auto victim = root / "must-preserve.txt";
    { std::ofstream out(victim); out << "original"; }
    std::error_code linkError;
    fs::create_symlink(victim, candidate, linkError);
    if (!linkError) {
        bool rejected = false;
        try { ccode::ResolveWorkspace(registry, root / "third"); }
        catch (const std::runtime_error& error) { rejected = std::string(error.what()) == "E_WORKSPACE_WRITE"; }
        assert(rejected);
        assert(readAll(victim) == "original");
        assert(readAll(registry) == saved);
        fs::remove(candidate);
    }
    fs::remove_all(root);
    std::cout << "session discovery passed\n";
}
