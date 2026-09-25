#include "../../scripts/ccode/sessions.hpp"
#include "../../scripts/ccode/snapshot.hpp"
#include "../../scripts/ccode/workspaces.hpp"
#include "../../scripts/ccode/session-index.hpp"
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
        {"isSidechain", false}, {"version", "2.1.221"}, {"message", {{"role", "user"}, {"content", "hello history"}}}}.dump() << '\n';
      }
    auto sessions = ccode::ListSessions(root / "projects", root);
    assert(sessions.size() == 1 && sessions[0].id == id);
    assert(sessions[0].title == "hello history");
    assert(ccode::ListSessions(root / "projects", root / "other").empty());
    // Existing transcripts beyond MAX_PATH must not silently disappear on restart.
    // Only the transcript exceeds 260; enumeration parents remain short enough.
    const auto longProjects = root / "long-projects";
    const auto longProject = longProjects / std::string(230 - longProjects.wstring().size(), 'x');
    fs::create_directories(ccode::SnapshotIoPath(longProject));
    const auto longTranscript = longProject / (id + ".jsonl");
    assert(longProject.wstring().size() < 260 && longTranscript.wstring().size() > 260);
    fs::copy_file(project / (id + ".jsonl"), ccode::SnapshotIoPath(longTranscript));
    std::cout << "Checking session discovery with transcript beyond 260 characters" << std::endl;
    auto longSessions = ccode::ListSessions(longProjects, root);
    assert(longSessions.size() == 1 && longSessions[0].id == id);
    fs::remove_all(ccode::SnapshotIoPath(longProjects));
    assert(!ccode::ValidSessionId("../../escape"));
    assert(!ccode::ValidSessionId("123"));
    // The frontend cache follows persistent workspace identity, while the native
    // transcript remains authoritative even when a cache is corrupt or deleted.
    const auto indexedProfile = root / "indexed-profile";
    const auto indexedProjects = indexedProfile / "home" / ".claude" / "projects";
    fs::create_directories(indexedProjects / "native-key");
    const auto source = indexedProjects / "native-key" / (id + ".jsonl");
    fs::copy_file(project / (id + ".jsonl"), source);
    auto indexed = ccode::ListWorkspaceSessions(indexedProfile, root);
    assert(indexed.size() == 1 && indexed[0].id == id);
    const auto workspaceIdentity = ccode::ResolveWorkspace(indexedProfile / "workspaces.json", root);
    const auto indexPath = indexedProfile / "session-index" / (workspaceIdentity + ".json");
    auto loadIndex = [&]() {
        std::ifstream input(indexPath);
        return ccode::Json::parse(input);
    };
    auto index = loadIndex();
    assert(index["schema"] == 1 && index["workspaceId"] == workspaceIdentity);
    assert(index["sessions"].size() == 1);
    assert(index["sessions"][0]["id"] == id);
    assert(index["sessions"][0]["summary"] == "hello history");
    assert(index["sessions"][0]["availability"] == "discovered");
    assert(index["sessions"][0].contains("modified"));
    assert(index["sessions"][0]["engineVersion"] == "2.1.221");
    { std::ofstream output(indexPath); output << "{broken cache"; }
    assert(ccode::ListWorkspaceSessions(indexedProfile, root).size() == 1);
    assert(loadIndex() == index);
    fs::remove(indexPath);
    assert(ccode::ListWorkspaceSessions(indexedProfile, root).size() == 1);
    assert(loadIndex() == index);
    // A known workspace's damaged session remains visible but cannot resume;
    // healthy sessions remain discoverable and the cache reflects native state.
    const std::string damagedId = "abcdefab-1234-1234-1234-123456789abc";
    const auto damaged = source.parent_path() / (damagedId + ".jsonl");
    const auto damagedBytes = ccode::Json{{"type", "user"}, {"sessionId", damagedId},
        {"cwd", root.u8string()}, {"message", {{"content", "private damaged title"}}}}.dump()
        + "\n{truncated-private-marker";
    { std::ofstream output(damaged, std::ios::binary); output << damagedBytes; }
    auto mixed = ccode::ListWorkspaceSessions(indexedProfile, root);
    assert(mixed.size() == 2);
    for (const auto& item : mixed) {
        if (item.id == damagedId) {
            assert(item.availability == "unavailable");
            assert(item.title == "Unavailable session");
            bool refused = false;
            try { ccode::RequireAvailableSession(mixed, damagedId); }
            catch (const std::runtime_error& error) { refused = std::string(error.what()) == "E_SESSION_DATA"; }
            assert(refused);
        } else {
            assert(item.id == id && item.availability == "discovered");
            ccode::RequireAvailableSession(mixed, id);
        }
    }
    const auto mixedIndex = loadIndex();
    for (const auto& entry : mixedIndex["sessions"])
        if (entry["id"] == damagedId) assert(entry["availability"] == "unavailable");
    { std::ifstream input(damaged, std::ios::binary);
      assert(std::string(std::istreambuf_iterator<char>(input), {}) == damagedBytes); }
    fs::remove(damaged);
    fs::remove(source);
    assert(ccode::ListWorkspaceSessions(indexedProfile, root).empty());
    assert(loadIndex()["sessions"].empty());
    // A long external profile must preserve identity and rebuild its cache on restart.
    const auto longProfile = root / std::string(150, 'p') / std::string(110, 'q');
    assert(longProfile.wstring().size() > 260);
    const auto longNative = longProfile / "home" / ".claude" / "projects" / "native-key";
    fs::create_directories(ccode::NativeIoPath(longNative));
    fs::copy_file(project / (id + ".jsonl"), ccode::NativeIoPath(longNative / (id + ".jsonl")));
    std::cout << "Checking workspace identity and rebuilt index beyond 260 characters" << std::endl;
    try {
    const auto longIdentity = ccode::ResolveWorkspace(longProfile / "workspaces.json", root);
    assert(ccode::ValidSessionId(longIdentity));
    const auto longListed = ccode::ListWorkspaceSessions(longProfile, root);
    assert(longListed.size() == 1 && longListed[0].id == id);
    const auto longIndex = ccode::NativeIoPath(longProfile / "session-index" / (longIdentity + ".json"));
    { std::ifstream input(longIndex); const auto document = ccode::Json::parse(input);
      assert(document["workspaceId"] == longIdentity && document["sessions"][0]["id"] == id); }
    fs::remove(longIndex);
    assert(ccode::ResolveWorkspace(longProfile / "workspaces.json", root) == longIdentity);
    assert(ccode::ListWorkspaceSessions(longProfile, root).size() == 1);
    assert(fs::exists(longIndex));
    } catch (const std::exception& error) {
        std::cerr << "Long profile identity/index failure: " << error.what() << std::endl;
        return 1;
    }
    fs::remove_all(ccode::NativeIoPath(root / std::string(150, 'p')));
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
    // Valid first-event metadata must not hide a corrupt later record.
    for (const auto& tail : {std::string("{truncated-private-marker"),
                             std::string("[]\n"), std::string("{broken}\n")}) {
        const auto bytes = validEvent.dump() + "\n" + tail;
        { std::ofstream out(transcript, std::ios::binary); out << bytes; }
        bool rejected = false;
        try { ccode::ListSessions(root / "projects", root); }
        catch (const std::runtime_error& error) {
            rejected = std::string(error.what()) == "E_SESSION_DATA";
        }
        assert(rejected);
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
