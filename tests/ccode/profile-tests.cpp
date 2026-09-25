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
    // Classify filesystem failures without leaking source paths or OS messages.
    const std::string deniedId = "82345678-1234-1234-1234-123456789abc";
    bool classifiedFilesystem = false;
    try {
        ccode::CreateProfileSnapshot(active, snapshots, deniedId, [](const fs::path&) -> std::string {
            throw fs::filesystem_error("private diagnostic", fs::path("private-source"),
                std::make_error_code(std::errc::permission_denied));
        });
    } catch (const std::runtime_error& error) {
        classifiedFilesystem = std::string(error.what()) == "E_SNAPSHOT_FS: hash-source: generic: " +
            std::to_string(static_cast<int>(std::errc::permission_denied));
    }
    assert(classifiedFilesystem);
    assert(!fs::exists(snapshots / deniedId));
    assert(fs::exists(snapshots / (deniedId + ".pending")));
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
    // Stage only a fully verified backup, never mutate it or the active source.
    const std::string verifiedId = "52345678-1234-1234-1234-123456789abc";
    auto testDigest = [](const fs::path& path) {
        return std::string(64, Read(path) == "source transcript\n" ? 'a' : 'b');
    };
    const auto verified = ccode::CreateProfileSnapshot(active, snapshots, verifiedId, testDigest);
    const auto verifiedManifest = ccode::VerifyProfileSnapshot(verified, testDigest);
    assert(verifiedManifest["snapshotId"] == verifiedId);
    const std::string candidateId = "62345678-1234-1234-1234-123456789abc";
    auto candidate = ccode::StageProfileCandidate(verified, root / "candidates", candidateId, testDigest);
    assert(Read(candidate / "profile/home/history.jsonl") == "source transcript\n");
    std::ifstream candidateInput(candidate / "candidate.json");
    const auto metadata = ccode::Json::parse(candidateInput);
    candidateInput.close();
    assert(metadata["state"] == "staged" && metadata["sourceSnapshotId"] == verifiedId);
    assert(!fs::exists(root / "active-profile.json"));
    Write(verified / "profile/home/history.jsonl", "tampered");
    bool tampered = false;
    try { ccode::VerifyProfileSnapshot(verified, testDigest); }
    catch (const std::runtime_error& error) { tampered = std::string(error.what()) == "E_SNAPSHOT_INTEGRITY"; }
    assert(tampered);
    assert(Read(candidate / "profile/home/history.jsonl") == "source transcript\n");
    assert(Read(active / "home/history.jsonl") == "source transcript\n");
    Write(verified / "profile/home/history.jsonl", "source transcript\n");
    auto mismatched = verifiedManifest;
    mismatched["snapshotId"] = candidateId;
    Write(verified / "manifest.json", mismatched.dump());
    bool wrongIdentity = false;
    try { ccode::VerifyProfileSnapshot(verified, testDigest); }
    catch (const std::runtime_error& error) { wrongIdentity = std::string(error.what()) == "E_SNAPSHOT_DATA"; }
    assert(wrongIdentity);
    // A valid short source can exceed MAX_PATH only after snapshot nesting.
    const auto longActive = root / "long-active";
    const auto longRelative = fs::path(std::string(70, 'a')) / std::string(70, 'b') / "history.jsonl";
    Write(longActive / longRelative, "source transcript\n");
    assert(Read(longActive / longRelative) == "source transcript\n");
    const auto longBackups = root / "long-snapshots" / std::string(45, 's');
    const std::string longId = "92345678-1234-1234-1234-123456789abc";
    assert((longBackups / (longId + ".pending") / "profile" / longRelative).native().size() > 260);
    std::cout << "Checking snapshot destination beyond 260 characters" << std::endl;
    const auto longSnapshot = ccode::CreateProfileSnapshot(longActive, longBackups, longId, testDigest);
    assert(ccode::VerifyProfileSnapshot(longSnapshot, testDigest)["files"].size() == 1);
    assert(Read(longActive / longRelative) == "source transcript\n");
    fs::remove_all(root);
    std::cout << "ccode profile recovery tests passed\n";
} catch (const std::exception& error) {
    std::cerr << "profile test exception: " << error.what() << '\n';
    return 1;
}
