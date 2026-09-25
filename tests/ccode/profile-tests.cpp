#include "../../scripts/ccode/profile.hpp"
#include "../../scripts/ccode/snapshot.hpp"

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

void Write(const fs::path& path, const std::string& value) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << value;
}

std::string Read(const fs::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main() try {
    const auto root = fs::temp_directory_path() / ("ccode-profile-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    const auto old = root / ".cc/projects/D--tt";
    const auto current = root / ".claude/projects/D--tt";
    Write(old / "history.jsonl", "old transcript\n");
    Write(old / "shared.jsonl", "old copy\n");
    Write(current / "shared.jsonl", "new transcript\n");
    Write(root / ".cc.json", "{\"projects\":{}}\n");
    const auto timestamp = fs::last_write_time(old / "history.jsonl");

    const auto result = ccode::RestoreLegacyProfile(root);
    assert(result.copied == 2);
    assert(result.skipped == 1);
    assert(Read(current / "history.jsonl") == "old transcript\n");
    assert(Read(current / "shared.jsonl") == "new transcript\n");
    assert(Read(old / "history.jsonl") == "old transcript\n");
    assert(Read(root / ".claude.json") == "{\"projects\":{}}\n");
    assert(fs::last_write_time(current / "history.jsonl") == timestamp);

    // Restarting must not resurrect a transcript the user subsequently deleted.
    fs::remove(current / "history.jsonl");
    assert(ccode::RestoreLegacyProfile(root).copied == 0);
    assert(!fs::exists(current / "history.jsonl"));

    // A fresh profile has nothing to migrate and must remain empty.
    const auto fresh = root / "fresh";
    fs::create_directory(fresh);
    assert(ccode::RestoreLegacyProfile(fresh).copied == 0);
    assert(fs::is_empty(fresh));

    // Never follow a destination symlink out of the portable profile.
    const auto linked = root / "linked";
    const auto outside = root / "outside";
    Write(linked / ".cc/projects/history.jsonl", "legacy");
    fs::create_directory(outside);
    fs::create_directory(linked / ".claude");
    std::error_code error;
    fs::create_directory_symlink(outside, linked / ".claude/projects", error);
    if (!error) {
        assert(ccode::RestoreLegacyProfile(linked).copied == 0);
        assert(fs::is_empty(outside));
    }
    const auto active = root / "active";
    Write(active / "home/history.jsonl", "source transcript\n");
    Write(active / "frontend.lock", "operational lock");
    fs::create_directories(active / "empty");
    const auto snapshots = root / "snapshots";
    const std::string snapshotId = "12345678-1234-1234-1234-123456789abc";
    // Inject a deterministic content fingerprint here; Windows CLI acceptance
    // independently checks the production SHA-256 against Python hashlib.
    auto fingerprint = [](const fs::path& path) { return Read(path); };
    auto snapshot = ccode::CreateProfileSnapshot(active, snapshots, snapshotId, fingerprint);
    assert(snapshot == snapshots / snapshotId);
    assert(Read(snapshot / "profile/home/history.jsonl") == "source transcript\n");
    assert(fs::is_directory(snapshot / "profile/empty"));
    assert(!fs::exists(snapshot / "profile/frontend.lock"));
    std::ifstream manifestInput(snapshot / "manifest.json");
    auto manifest = ccode::Json::parse(manifestInput);
    manifestInput.close();
    assert(manifest["schema"] == 1 && manifest["snapshotId"] == snapshotId);
    assert(manifest["files"]["home/history.jsonl"]["sha256"] == "source transcript\n");
    assert(Read(active / "home/history.jsonl") == "source transcript\n");
    bool conflict = false;
    try { ccode::CreateProfileSnapshot(active, snapshots, snapshotId, fingerprint); }
    catch (const std::runtime_error& error) { conflict = std::string(error.what()) == "E_SNAPSHOT_EXISTS"; }
    assert(conflict);
    const std::string interruptedId = "22345678-1234-1234-1234-123456789abc";
    bool interrupted = false;
    try {
        ccode::CreateProfileSnapshot(active, snapshots, interruptedId, [](const fs::path&) -> std::string {
            throw std::runtime_error("injected read failure");
        });
    } catch (const std::runtime_error&) { interrupted = true; }
    assert(interrupted && !fs::exists(snapshots / interruptedId));
    assert(fs::exists(snapshots / (interruptedId + ".pending")));
    assert(Read(active / "home/history.jsonl") == "source transcript\n");
    const std::string changedId = "32345678-1234-1234-1234-123456789abc";
    bool changed = false;
    try {
        ccode::CreateProfileSnapshot(active, snapshots, changedId, [&](const fs::path& path) {
            if (path == active / "home/history.jsonl") return Read(path);
            return std::string("injected copy corruption");
        });
    } catch (const std::runtime_error& error) { changed = std::string(error.what()) == "E_SNAPSHOT_CHANGED"; }
    assert(changed && !fs::exists(snapshots / changedId));
    assert(Read(snapshot / "profile/home/history.jsonl") == "source transcript\n");
    const std::string retryId = "42345678-1234-1234-1234-123456789abc";
    assert(fs::exists(ccode::CreateProfileSnapshot(active, snapshots, retryId, fingerprint)));
    fs::remove_all(root);
    std::cout << "ccode profile recovery tests passed\n";
} catch (const std::exception& error) {
    std::cerr << "profile test exception: " << error.what() << '\n';
    return 1;
}
