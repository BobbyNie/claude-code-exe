#include "../../scripts/ccode/profile.hpp"

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;

void Write(const fs::path& path, const std::string& value) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << value;
}

std::string Read(const fs::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main() {
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
    fs::remove_all(root);
    std::cout << "ccode profile recovery tests passed\n";
}
