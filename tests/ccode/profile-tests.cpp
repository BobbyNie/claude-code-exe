#include "../../scripts/ccode/profile.hpp"
#include "../../scripts/ccode/snapshot.hpp"
#include "../../scripts/ccode/candidate.hpp"

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
    const auto pointerIo = root / "pointer-io";
    fs::create_directories(pointerIo);
    ccode::CommitActiveProfilePointer(pointerIo, "first pointer\n");
    assert(Read(pointerIo / "active-profile.json") == "first pointer\n");
    ccode::CommitActiveProfilePointer(pointerIo, "second pointer\n");
    assert(Read(pointerIo / "active-profile.json") == "second pointer\n");
    assert(!fs::exists(pointerIo / "active-profile.json.pending"));
    bool missingPointerDirectory = false;
    try { ccode::CommitActiveProfilePointer(root / "missing-pointer-dir", "must not commit"); }
    catch (const std::runtime_error& error) { missingPointerDirectory = std::string(error.what()) == "E_ACTIVATION_WRITE"; }
    assert(missingPointerDirectory);
    assert(!fs::exists(root / "missing-pointer-dir"));
    Write(pointerIo / "active-profile.json.pending", "previous evidence");
    bool commitConflict = false;
    try { ccode::CommitActiveProfilePointer(pointerIo, "replacement"); }
    catch (const std::runtime_error& error) { commitConflict = std::string(error.what()) == "E_ACTIVATION_PENDING"; }
    assert(commitConflict);
    assert(Read(pointerIo / "active-profile.json.pending") == "previous evidence");
    assert(Read(pointerIo / "active-profile.json") == "second pointer\n");
    const auto obstructedPointer = root / "obstructed-pointer";
    fs::create_directories(obstructedPointer / "active-profile.json");
    Write(obstructedPointer / "active-profile.json" / "retain.txt", "unrelated directory");
    bool commitReplaceFailed = false;
    try { ccode::CommitActiveProfilePointer(obstructedPointer, "uncommitted pointer\n"); }
    catch (const std::runtime_error& error) { commitReplaceFailed = std::string(error.what()) == "E_ACTIVATION_WRITE"; }
    assert(commitReplaceFailed);
    assert(Read(obstructedPointer / "active-profile.json.pending") == "uncommitted pointer\n");
    assert(Read(obstructedPointer / "active-profile.json" / "retain.txt") == "unrelated directory");
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
            if (fs::equivalent(path, active / "home/history.jsonl")) return Read(path);
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
    const auto longCandidate = ccode::StageProfileCandidate(longSnapshot,
        root / "long-candidates" / std::string(45, 'c'), candidateId, testDigest);
    assert(Read(ccode::SnapshotIoPath(longCandidate / "profile" / longRelative)) == "source transcript\n");
    assert(Read(longActive / longRelative) == "source transcript\n");
    // Validation must be tied to real recovered history, engine and frozen bytes.
    const auto candidateProfile = root / "probe-source";
    const auto historyId = "a2345678-1234-1234-1234-123456789abc";
    Write(candidateProfile / "home/.claude/projects/work" / (std::string(historyId) + ".jsonl"),
        ccode::Json{{"type", "user"}, {"sessionId", historyId}, {"cwd", root.u8string()},
            {"message", {{"role", "user"}, {"content", "private historical marker"}}}}.dump() + "\n");
    const auto probeSnapshot = ccode::CreateProfileSnapshot(candidateProfile, snapshots, historyId, testDigest);
    const auto probeCandidate = ccode::StageProfileCandidate(probeSnapshot, root / "probe-candidates", historyId, testDigest);
    // The public command holds a live profile lock, excluded from snapshot data.
    Write(probeCandidate / "profile/frontend.lock", "operational lock");
    assert(ccode::CandidateHistoryText(probeCandidate / "profile", root, historyId) == "private historical marker");
    const ccode::Json engine = {{"version", "test-engine"}, {"sha256", std::string(64, 'e')}};
    Write(candidateProfile / "new-history.jsonl", "new active data after snapshot");
    bool sourceConflict = false, ranConflictProbe = false;
    try {
        ccode::ValidateProfileCandidate(probeCandidate, probeSnapshot, candidateProfile, root / "verified", snapshotId,
            root, historyId, engine, testDigest, [&](const fs::path&, const std::string&, const std::string&) {
                ranConflictProbe = true; return true;
            });
    } catch (const std::runtime_error& error) { sourceConflict = std::string(error.what()) == "E_SOURCE_CHANGED"; }
    assert(sourceConflict && !ranConflictProbe);
    assert(!fs::exists(probeCandidate / "validation.json"));
    assert(Read(candidateProfile / "new-history.jsonl") == "new active data after snapshot");
    fs::remove(candidateProfile / "new-history.jsonl");
    // Ignoring the operational lock must not permit any other extra file.
    Write(probeCandidate / "profile/unexpected.bin", "unexpected");
    bool rejectedExtra = false, ranUnexpectedProbe = false;
    try {
        ccode::ValidateProfileCandidate(probeCandidate, probeSnapshot, candidateProfile, root / "verified", snapshotId,
            root, historyId, engine, testDigest, [&](const fs::path&, const std::string&, const std::string&) {
                ranUnexpectedProbe = true; return true;
            });
    } catch (const std::runtime_error& error) { rejectedExtra = std::string(error.what()) == "E_CANDIDATE_CHANGED"; }
    assert(rejectedExtra && !ranUnexpectedProbe);
    fs::remove(probeCandidate / "profile/unexpected.bin");
    Write(probeSnapshot / "profile/frontend.lock", "unexpected snapshot lock");
    bool rejectedSnapshotLock = false;
    try { ccode::VerifyProfileSnapshot(probeSnapshot, testDigest); }
    catch (const std::runtime_error& error) { rejectedSnapshotLock = std::string(error.what()) == "E_SNAPSHOT_INTEGRITY"; }
    assert(rejectedSnapshotLock);
    fs::remove(probeSnapshot / "profile/frontend.lock");
    bool failedProbe = false;
    try {
        ccode::ValidateProfileCandidate(probeCandidate, probeSnapshot, candidateProfile, root / "verified", snapshotId,
            root, historyId, engine, testDigest, [](const fs::path&, const std::string&, const std::string&) { return false; });
    } catch (const std::runtime_error& error) { failedProbe = std::string(error.what()) == "E_CANDIDATE_HISTORY"; }
    assert(failedProbe && !fs::exists(probeCandidate / "validation.json"));
    bool changedDuringProbe = false;
    try {
        ccode::ValidateProfileCandidate(probeCandidate, probeSnapshot, candidateProfile, root / "verified", snapshotId,
            root, historyId, engine, testDigest, [&](const fs::path&, const std::string&, const std::string&) {
                Write(candidateProfile / "external-write.txt", "preserve this write");
                return true;
            });
    } catch (const std::runtime_error& error) { changedDuringProbe = std::string(error.what()) == "E_SOURCE_CHANGED"; }
    assert(changedDuringProbe && !fs::exists(probeCandidate / "validation.json"));
    assert(Read(candidateProfile / "external-write.txt") == "preserve this write");
    fs::remove(candidateProfile / "external-write.txt");
    // Evidence appearing during an engine probe must never be truncated.
    const auto lateCandidate = ccode::StageProfileCandidate(probeSnapshot, root / "late-candidates", historyId, testDigest);
    bool latePendingRejected = false;
    const auto pendingEvidence = lateCandidate / "validation.json.pending";
    try {
        ccode::ValidateProfileCandidate(lateCandidate, probeSnapshot, candidateProfile, root / "verified",
            "e2345678-1234-1234-1234-123456789abc", root, historyId, engine, testDigest,
            [&](const fs::path&, const std::string&, const std::string&) {
                Write(pendingEvidence, "preserve interrupted validation evidence");
                return true;
            });
    } catch (const std::runtime_error& error) {
        latePendingRejected = std::string(error.what()) == "E_CANDIDATE_WRITE";
    }
    assert(Read(pendingEvidence) == "preserve interrupted validation evidence");
    assert(latePendingRejected && !fs::exists(lateCandidate / "validation.json"));
    fs::remove(pendingEvidence); // Only remove evidence created by this test.
    // A completed receipt appearing during the probe is also immutable evidence.
    const auto racedCandidate = ccode::StageProfileCandidate(probeSnapshot,
        root / "published-candidates", historyId, testDigest);
    bool publishedReceiptRejected = false;
    try {
        ccode::ValidateProfileCandidate(racedCandidate, probeSnapshot, candidateProfile, root / "verified",
            "f2345678-1234-1234-1234-123456789abc", root, historyId, engine, testDigest,
            [&](const fs::path&, const std::string&, const std::string&) {
                Write(racedCandidate / "validation.json", "retain existing completed receipt");
                return true;
            });
    } catch (const std::runtime_error& error) {
        publishedReceiptRejected = std::string(error.what()) == "E_CANDIDATE_WRITE";
    }
    assert(Read(racedCandidate / "validation.json") == "retain existing completed receipt");
    assert(publishedReceiptRejected);
    assert(ccode::ReadCandidateDocument(racedCandidate / "validation.json.pending")["historyVerified"] == true);
    bool sawPrivateHistory = false;
    auto receipt = ccode::ValidateProfileCandidate(probeCandidate, probeSnapshot, candidateProfile, root / "verified", snapshotId,
        root, historyId, engine, testDigest, [&](const fs::path& profile, const std::string& id, const std::string& expected) {
            sawPrivateHistory = fs::equivalent(profile, probeCandidate / "profile") && id == historyId && expected == "private historical marker";
            Write(profile / "engine-recovered.txt", "source transcript\n");
            return true;
        });
    assert(sawPrivateHistory && receipt["scope"] == "single-session");
    assert(receipt["engine"] == engine && receipt["sessionId"] == historyId);
    assert(receipt.dump().find("private historical marker") == std::string::npos);
    assert(ccode::VerifyCandidateValidation(probeCandidate, root / "verified", engine, testDigest) == receipt);
    bool singleCannotActivate = false;
    try { ccode::VerifyCandidateActivation(probeCandidate, probeSnapshot, candidateProfile,
        root / "verified", engine, testDigest); }
    catch (const std::runtime_error& error) { singleCannotActivate = std::string(error.what()) == "E_ACTIVATION_SCOPE"; }
    assert(singleCannotActivate);
    const auto candidateMetadata = ccode::ReadCandidateDocument(probeCandidate / "candidate.json");
    for (const auto& invalid : std::vector<ccode::Json>{
        {{"schema", 2}}, {{"schema", nullptr}}, {{"state", "active"}}, {{"state", nullptr}}}) {
        auto changedMetadata = candidateMetadata;
        changedMetadata.update(invalid);
        Write(probeCandidate / "candidate.json", changedMetadata.dump());
        bool rejectedMetadata = false;
        try { ccode::VerifyCandidateValidation(probeCandidate, root / "verified", engine, testDigest); }
        catch (const std::runtime_error& error) { rejectedMetadata = std::string(error.what()) == "E_CANDIDATE_DATA"; }
        assert(rejectedMetadata);
        assert(ccode::ReadCandidateDocument(probeCandidate / "validation.json") == receipt);
    }
    Write(probeCandidate / "candidate.json", candidateMetadata.dump());
    assert(ccode::VerifyCandidateValidation(probeCandidate, root / "verified", engine, testDigest) == receipt);
    auto incompatibleEngine = engine;
    incompatibleEngine["sha256"] = std::string(64, 'f');
    bool incompatible = false;
    try { ccode::VerifyCandidateValidation(probeCandidate, root / "verified", incompatibleEngine, testDigest); }
    catch (const std::runtime_error& error) { incompatible = std::string(error.what()) == "E_CANDIDATE_DATA"; }
    assert(incompatible);
    Write(probeCandidate / "profile/engine-recovered.txt", "changed after verification");
    bool staleReceipt = false;
    try { ccode::VerifyCandidateValidation(probeCandidate, root / "verified", engine, testDigest); }
    catch (const std::runtime_error& error) { staleReceipt = std::string(error.what()) == "E_CANDIDATE_CHANGED"; }
    assert(staleReceipt);
    // Inventory is derived from all native projects, not the caller's cwd/cache.
    const auto inventoryProfile = root / "inventory-source";
    const auto workspaceTwo = root / "second workspace";
    fs::create_directories(workspaceTwo);
    const std::string secondId = "b2345678-1234-1234-1234-123456789abc";
    fs::copy(candidateProfile, inventoryProfile, fs::copy_options::recursive);
    const auto secondTranscript = inventoryProfile / "home/.claude/projects/second" / (secondId + ".jsonl");
    Write(secondTranscript, ccode::Json{{"type", "user"}, {"sessionId", secondId}, {"cwd", workspaceTwo.u8string()},
        {"message", {{"role", "user"}, {"content", "second private marker"}}}}.dump() + "\n");
    const auto inventory = ccode::CandidateSessionInventory(inventoryProfile);
    assert(inventory.size() == 2);
    assert(inventory[0].session == historyId && ccode::SameWorkspace(inventory[0].workspace, root));
    assert(inventory[1].session == secondId && ccode::SameWorkspace(inventory[1].workspace, workspaceTwo));
    const auto invalidLegacy = inventoryProfile / "home/.claude/projects/legacy/old-session.jsonl";
    Write(invalidLegacy, "legacy-session-marker");
    bool unknownTranscript = false;
    try { ccode::CandidateSessionInventory(inventoryProfile); }
    catch (const std::runtime_error& error) { unknownTranscript = std::string(error.what()) == "E_CANDIDATE_HISTORY"; }
    assert(unknownTranscript && Read(invalidLegacy) == "legacy-session-marker");
    fs::remove(invalidLegacy);
    const auto secondSaved = Read(secondTranscript);
    Write(secondTranscript, secondSaved + "{truncated");
    bool badInventory = false;
    try { ccode::CandidateSessionInventory(inventoryProfile); }
    catch (const std::runtime_error& error) { badInventory = std::string(error.what()) == "E_CANDIDATE_HISTORY"; }
    assert(badInventory && Read(secondTranscript) == secondSaved + "{truncated");
    Write(secondTranscript, secondSaved);
    const auto duplicateTranscript = inventoryProfile / "home/.claude/projects/duplicate" / (secondId + ".jsonl");
    Write(duplicateTranscript, secondSaved);
    bool duplicateInventory = false;
    try { ccode::CandidateSessionInventory(inventoryProfile); }
    catch (const std::runtime_error& error) { duplicateInventory = std::string(error.what()) == "E_CANDIDATE_HISTORY"; }
    assert(duplicateInventory);
    fs::remove(duplicateTranscript);
    const auto allSnapshot = ccode::CreateProfileSnapshot(inventoryProfile, root / "all-snapshots", historyId, testDigest);
    const auto allCandidate = ccode::StageProfileCandidate(allSnapshot, root / "all-candidates", historyId, testDigest);
    size_t failedProbeCount = 0;
    bool failedAll = false;
    try {
        ccode::ValidateProfileCandidate(allCandidate, allSnapshot, inventoryProfile,
            root / "all-verified", snapshotId, root, "", engine, testDigest, {},
            [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                return ++failedProbeCount == 1;
            });
    } catch (const std::runtime_error& error) { failedAll = std::string(error.what()) == "E_CANDIDATE_HISTORY"; }
    assert(failedAll && failedProbeCount == 2 && !fs::exists(allCandidate / "validation.json"));
    std::vector<std::string> probedSessions;
    const auto allReceipt = ccode::ValidateProfileCandidate(allCandidate, allSnapshot, inventoryProfile,
        root / "all-verified", snapshotId, root, "", engine, testDigest, {},
        [&](const fs::path& isolated, const fs::path& cwd, const std::string& id, const std::string& expected) {
            assert(fs::equivalent(isolated, allCandidate / "profile"));
            assert(ccode::SameWorkspace(cwd, id == historyId ? root : workspaceTwo));
            assert(expected == (id == historyId ? "private historical marker" : "second private marker"));
            probedSessions.push_back(id);
            return true;
        });
    assert(probedSessions == std::vector<std::string>({historyId, secondId}));
    assert(allReceipt["scope"] == "all-top-level-sessions" && allReceipt["sessions"].size() == 2);
    assert(!allReceipt.contains("sessionId") && allReceipt.dump().find("private marker") == std::string::npos);
    assert(ccode::VerifyCandidateValidation(allCandidate, root / "all-verified", engine, testDigest) == allReceipt);
    assert(ccode::VerifyCandidateActivation(allCandidate, allSnapshot, inventoryProfile,
        root / "all-verified", engine, testDigest) == allReceipt);
    Write(inventoryProfile / "after-validation.txt", "new source data must survive");
    bool activationSourceChanged = false;
    try { ccode::VerifyCandidateActivation(allCandidate, allSnapshot, inventoryProfile,
        root / "all-verified", engine, testDigest); }
    catch (const std::runtime_error& error) { activationSourceChanged = std::string(error.what()) == "E_SOURCE_CHANGED"; }
    assert(activationSourceChanged);
    assert(Read(inventoryProfile / "after-validation.txt") == "new source data must survive");
    assert(ccode::ReadCandidateDocument(allCandidate / "validation.json") == allReceipt);
    fs::remove(inventoryProfile / "after-validation.txt");
    const auto selectionRoot = root / "selection";
    fs::create_directories(selectionRoot);
    assert(ccode::ResolveActiveProfile(selectionRoot, engine) == selectionRoot / "profile");
    Write(selectionRoot / "active-profile.json", "{broken");
    bool badPointer = false;
    try { ccode::ResolveActiveProfile(selectionRoot, engine); }
    catch (const std::runtime_error& error) { badPointer = std::string(error.what()) == "E_ACTIVE_PROFILE"; }
    assert(badPointer && Read(selectionRoot / "active-profile.json") == "{broken");
    fs::create_directories(selectionRoot / "candidates");
    fs::copy(allCandidate, selectionRoot / "candidates" / historyId, fs::copy_options::recursive);
    const ccode::Json pointer = {{"schema", 1}, {"candidateId", historyId},
        {"verificationId", snapshotId}, {"sourceSnapshotId", historyId},
        {"engine", engine}, {"adapter", "stream-json-v1"}};
    Write(selectionRoot / "active-profile.json", pointer.dump());
    const auto selected = selectionRoot / "candidates" / historyId / "profile";
    assert(ccode::ResolveActiveProfile(selectionRoot, engine) == selected);
    bool otherEngineRejected = false;
    try { ccode::ResolveActiveProfile(selectionRoot, incompatibleEngine); }
    catch (const std::runtime_error& error) { otherEngineRejected = std::string(error.what()) == "E_ACTIVE_PROFILE"; }
    assert(otherEngineRejected);
    assert(ccode::ResolveProfileForBackup(selectionRoot) == selected);
    // A live profile changes after activation; selection must not compare its
    // bytes to the immutable activation receipt on every normal restart.
    Write(selected / "new-live-turn.txt", "later session data");
    assert(ccode::ResolveActiveProfile(selectionRoot, engine) == selected);
    for (const auto& patch : std::vector<ccode::Json>{
        {{"schema", 2}}, {{"schema", nullptr}}, {{"candidateId", "../escape"}},
        {{"verificationId", historyId}}, {{"adapter", "unknown"}}, {{"engine", incompatibleEngine}}}) {
        auto invalidPointer = pointer;
        invalidPointer.update(patch);
        Write(selectionRoot / "active-profile.json", invalidPointer.dump());
        bool rejected = false;
        try { ccode::ResolveActiveProfile(selectionRoot, engine); }
        catch (const std::runtime_error& error) { rejected = std::string(error.what()) == "E_ACTIVE_PROFILE"; }
        assert(rejected && Read(selected / "new-live-turn.txt") == "later session data");
        bool invalidBackupRejected = false;
        try { ccode::ResolveProfileForBackup(selectionRoot); }
        catch (const std::runtime_error& error) { invalidBackupRejected = std::string(error.what()) == "E_ACTIVE_PROFILE"; }
        assert(invalidBackupRejected);
    }
    Write(selectionRoot / "active-profile.json", pointer.dump());
    fs::rename(selected, selected.parent_path() / "missing-profile");
    bool missingActive = false;
    try { ccode::ResolveActiveProfile(selectionRoot, engine); }
    catch (const std::runtime_error& error) { missingActive = std::string(error.what()) == "E_ACTIVE_PROFILE"; }
    assert(missingActive);
    const auto activationRoot = root / "activation";
    fs::create_directories(activationRoot / "candidates");
    fs::create_directories(activationRoot / "snapshots");
    fs::create_directories(activationRoot / "verified");
    fs::copy(inventoryProfile, activationRoot / "profile", fs::copy_options::recursive);
    fs::copy(allCandidate, activationRoot / "candidates" / historyId, fs::copy_options::recursive);
    fs::copy(allSnapshot, activationRoot / "snapshots" / historyId, fs::copy_options::recursive);
    fs::copy(root / "all-verified" / snapshotId, activationRoot / "verified" / snapshotId, fs::copy_options::recursive);
    const auto beforeActivation = ccode::CandidateFiles(activationRoot / "profile", testDigest);
    Write(activationRoot / "active-profile.json.pending", "interrupted evidence");
    bool pendingRejected = false;
    try { ccode::ActivateProfileCandidate(activationRoot, historyId, engine, testDigest); }
    catch (const std::runtime_error& error) { pendingRejected = std::string(error.what()) == "E_ACTIVATION_PENDING"; }
    assert(pendingRejected && !fs::exists(activationRoot / "active-profile.json"));
    assert(Read(activationRoot / "active-profile.json.pending") == "interrupted evidence");
    fs::remove(activationRoot / "active-profile.json.pending");
    ccode::ActivateProfileCandidate(activationRoot, historyId, engine, testDigest);
    assert(ccode::ResolveActiveProfile(activationRoot, engine) == activationRoot / "candidates" / historyId / "profile");
    assert(ccode::CandidateFiles(activationRoot / "profile", testDigest) == beforeActivation);
    assert(!fs::exists(activationRoot / "active-profile.json.pending"));
    // A second activation must replace an existing pointer, not only create one.
    const auto firstActive = ccode::ResolveActiveProfile(activationRoot, engine);
    Write(firstActive / "later-source.txt", "preserve first active writes");
    const auto firstActiveFiles = ccode::CandidateFiles(firstActive, testDigest);
    const auto nextSnapshot = ccode::CreateProfileSnapshot(firstActive, activationRoot / "snapshots", secondId, testDigest);
    const auto nextCandidate = ccode::StageProfileCandidate(nextSnapshot, activationRoot / "candidates", secondId, testDigest);
    ccode::ValidateProfileCandidate(nextCandidate, nextSnapshot, firstActive,
        activationRoot / "verified", secondId, root, "", engine, testDigest, {},
        [](const fs::path&, const fs::path&, const std::string&, const std::string&) { return true; });
    const auto oldPointerBytes = Read(activationRoot / "active-profile.json");
    Write(activationRoot / "active-profile.json.pending", "interrupted second activation");
    bool secondPendingRejected = false;
    try { ccode::ActivateProfileCandidate(activationRoot, secondId, engine, testDigest); }
    catch (const std::runtime_error& error) { secondPendingRejected = std::string(error.what()) == "E_ACTIVATION_PENDING"; }
    assert(secondPendingRejected && Read(activationRoot / "active-profile.json") == oldPointerBytes);
    assert(ccode::ResolveActiveProfile(activationRoot, engine) == firstActive);
    assert(Read(activationRoot / "active-profile.json.pending") == "interrupted second activation");
    const auto archivedPending = ccode::ArchiveActivationPending(activationRoot, snapshotId);
    assert(Read(archivedPending) == "interrupted second activation");
    assert(!fs::exists(activationRoot / "active-profile.json.pending"));
    assert(Read(activationRoot / "active-profile.json") == oldPointerBytes);
    // Recovery refuses collisions, invalid IDs and missing evidence without
    // touching the committed pointer or previously archived bytes.
    Write(activationRoot / "active-profile.json.pending", "new pending");
    bool recoveryCollision = false;
    try { ccode::ArchiveActivationPending(activationRoot, snapshotId); }
    catch (const std::runtime_error& error) { recoveryCollision = std::string(error.what()) == "E_ACTIVATION_RECOVERY_EXISTS"; }
    assert(recoveryCollision);
    assert(Read(archivedPending) == "interrupted second activation");
    assert(Read(activationRoot / "active-profile.json.pending") == "new pending");
    bool invalidRecovery = false;
    try { ccode::ArchiveActivationPending(activationRoot, "../escape"); }
    catch (const std::runtime_error& error) { invalidRecovery = std::string(error.what()) == "E_ACTIVATION_RECOVERY_ID"; }
    assert(invalidRecovery);
    ccode::ArchiveActivationPending(activationRoot, historyId);
    bool missingPending = false;
    try { ccode::ArchiveActivationPending(activationRoot, secondId); }
    catch (const std::runtime_error& error) { missingPending = std::string(error.what()) == "E_ACTIVATION_PENDING_MISSING"; }
    assert(missingPending);
    assert(!fs::exists(activationRoot / "activation-recovery" / secondId));
    assert(Read(activationRoot / "active-profile.json") == oldPointerBytes);
    const auto invalidRecoveryRoot = root / "invalid-recovery";
    fs::create_directories(invalidRecoveryRoot / "active-profile.json.pending");
    bool directoryPending = false;
    try { ccode::ArchiveActivationPending(invalidRecoveryRoot, snapshotId); }
    catch (const std::runtime_error& error) { directoryPending = std::string(error.what()) == "E_ACTIVATION_PENDING_INVALID"; }
    assert(directoryPending && !fs::exists(invalidRecoveryRoot / "activation-recovery"));
    const auto linkedRecoveryRoot = root / "linked-recovery";
    Write(linkedRecoveryRoot / "active-profile.json.pending", "preserve me");
    std::error_code linkError;
    fs::create_directory_symlink(activationRoot / "activation-recovery", linkedRecoveryRoot / "activation-recovery", linkError);
    if (!linkError) {
        bool linkedArchive = false;
        try { ccode::ArchiveActivationPending(linkedRecoveryRoot, secondId); }
        catch (const std::runtime_error& error) { linkedArchive = std::string(error.what()) == "E_ACTIVATION_RECOVERY_INVALID"; }
        assert(linkedArchive && Read(linkedRecoveryRoot / "active-profile.json.pending") == "preserve me");
    }
    ccode::ActivateProfileCandidate(activationRoot, secondId, engine, testDigest);
    assert(ccode::ResolveActiveProfile(activationRoot, engine) == nextCandidate / "profile");
    assert(ccode::CandidateFiles(firstActive, testDigest) == firstActiveFiles);
    const auto secondPointerBytes = Read(activationRoot / "active-profile.json");
    bool alreadyActive = false;
    try { ccode::ActivateProfileCandidate(activationRoot, secondId, engine, testDigest); }
    catch (const std::runtime_error& error) { alreadyActive = std::string(error.what()) == "E_ACTIVATION_ALREADY_ACTIVE"; }
    assert(alreadyActive && Read(activationRoot / "active-profile.json") == secondPointerBytes);
    // Preparing an older rollback must preserve writes made since that snapshot.
    const std::string preservationId = "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
    const auto currentFiles = ccode::CandidateFiles(nextCandidate / "profile", testDigest);
    assert(currentFiles != ccode::VerifyProfileSnapshot(activationRoot / "snapshots" / historyId,
        testDigest)["files"]);
    const auto rollback = ccode::PrepareProfileRollback(activationRoot, historyId,
        historyId, preservationId, incompatibleEngine, testDigest);
    assert(rollback["state"] == "prepared");
    assert(rollback["priorActivePointer"] == ccode::Json::parse(secondPointerBytes));
    assert(rollback["targetEngine"] == incompatibleEngine);
    assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    assert(ccode::CandidateFiles(nextCandidate / "profile", testDigest) == currentFiles);
    assert(ccode::VerifyProfileSnapshot(activationRoot / "snapshots" / preservationId,
        testDigest)["files"] == currentFiles);
    assert(ccode::CandidateFiles(activationRoot / "rollback-candidates" / historyId / "profile",
        testDigest) == ccode::VerifyProfileSnapshot(activationRoot / "snapshots" / historyId,
        testDigest)["files"]);
    assert(ccode::ReadCandidateDocument(activationRoot / "rollback-candidates" / historyId /
        "rollback.json") == rollback);
    const std::string unusedPreservation = "dddddddd-dddd-4ddd-8ddd-dddddddddddd";
    bool rollbackCollision = false;
    try { ccode::PrepareProfileRollback(activationRoot, historyId, historyId,
        unusedPreservation, engine, testDigest); }
    catch (const std::runtime_error& error) {
        rollbackCollision = std::string(error.what()) == "E_ROLLBACK_EXISTS";
    }
    assert(rollbackCollision);
    assert(!fs::exists(activationRoot / "snapshots" / unusedPreservation));
    const auto linkedRollbackRoot = root / "linked-rollback";
    fs::create_directories(linkedRollbackRoot);
    linkError.clear();
    fs::create_directory_symlink(activationRoot / "snapshots",
        linkedRollbackRoot / "snapshots", linkError);
    if (!linkError) {
        bool rejectedLinkedSource = false;
        try { ccode::PrepareProfileRollback(linkedRollbackRoot, historyId, secondId,
            unusedPreservation, engine, testDigest); }
        catch (const std::runtime_error& error) {
            rejectedLinkedSource = std::string(error.what()) == "E_ROLLBACK_PATH";
        }
        assert(rejectedLinkedSource);
        assert(!fs::exists(activationRoot / "snapshots" / unusedPreservation));
    }
    for (const auto& invalidSource : {std::string("../escape"), unusedPreservation}) {
        const auto beforeSnapshots = ccode::CandidateFiles(activationRoot / "snapshots", testDigest);
        bool rejectedSource = false;
        try { ccode::PrepareProfileRollback(activationRoot, invalidSource, secondId,
            unusedPreservation, engine, testDigest); }
        catch (const std::runtime_error&) { rejectedSource = true; }
        assert(rejectedSource);
        assert(ccode::CandidateFiles(activationRoot / "snapshots", testDigest) == beforeSnapshots);
        assert(!fs::exists(activationRoot / "rollback-candidates" / secondId));
        assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    }
    // Rollback validation probes every old session, not the newer active data.
    size_t rejectedProbes = 0;
    bool wrongRollbackEngine = false;
    try {
        ccode::ValidateProfileRollback(activationRoot, historyId, snapshotId, engine, testDigest,
            [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                ++rejectedProbes; return true;
            });
    } catch (const std::runtime_error& error) {
        wrongRollbackEngine = std::string(error.what()) == "E_ROLLBACK_DATA";
    }
    assert(wrongRollbackEngine && rejectedProbes == 0);
    assert(!fs::exists(activationRoot / "rollback-candidates" / historyId / "validation.json"));
    Write(nextCandidate / "profile/after-preparation.txt", "must not be lost");
    bool changedRollbackActive = false;
    try {
        ccode::ValidateProfileRollback(activationRoot, historyId, snapshotId, incompatibleEngine, testDigest,
            [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                ++rejectedProbes; return true;
            });
    } catch (const std::runtime_error& error) {
        changedRollbackActive = std::string(error.what()) == "E_ROLLBACK_CHANGED";
    }
    assert(changedRollbackActive && rejectedProbes == 0);
    assert(Read(nextCandidate / "profile/after-preparation.txt") == "must not be lost");
    assert(!fs::exists(activationRoot / "rollback-candidates" / historyId / "validation.json"));
    fs::remove(nextCandidate / "profile/after-preparation.txt");
    size_t interruptedProbes = 0;
    bool changedDuringRollback = false;
    try {
        ccode::ValidateProfileRollback(activationRoot, historyId, snapshotId, incompatibleEngine, testDigest,
            [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                ++interruptedProbes;
                Write(nextCandidate / "profile/during-validation.txt", "retain newest bytes");
                return true;
            });
    } catch (const std::runtime_error& error) {
        changedDuringRollback = std::string(error.what()) == "E_ROLLBACK_CHANGED";
    }
    assert(changedDuringRollback && interruptedProbes == 1);
    assert(Read(nextCandidate / "profile/during-validation.txt") == "retain newest bytes");
    assert(!fs::exists(activationRoot / "rollback-candidates" / historyId / "validation.json"));
    assert(!fs::exists(activationRoot / "rollback-candidates" / historyId / "rollback-validation.json"));
    fs::remove(nextCandidate / "profile/during-validation.txt");
    if (!linkError) {
        fs::create_directory_symlink(activationRoot / "rollback-candidates",
            linkedRollbackRoot / "rollback-candidates", linkError);
        if (!linkError) {
            bool rejectedLinkedValidation = false;
            try {
                ccode::ValidateProfileRollback(linkedRollbackRoot, historyId, snapshotId,
                    incompatibleEngine, testDigest,
                    [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                        ++rejectedProbes; return true;
                    });
            } catch (const std::runtime_error& error) {
                rejectedLinkedValidation = std::string(error.what()) == "E_ROLLBACK_PATH";
            }
            assert(rejectedLinkedValidation && rejectedProbes == 0);
        }
    }
    const auto rollbackPending = activationRoot / "rollback-candidates" / historyId /
        "rollback-validation.json.pending";
    Write(rollbackPending, "interrupted validation evidence");
    bool pendingRollbackValidation = false;
    try {
        ccode::ValidateProfileRollback(activationRoot, historyId, snapshotId, incompatibleEngine, testDigest,
            [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                ++rejectedProbes; return true;
            });
    } catch (const std::runtime_error& error) {
        pendingRollbackValidation = std::string(error.what()) == "E_ROLLBACK_EXISTS";
    }
    assert(pendingRollbackValidation && rejectedProbes == 0);
    assert(Read(rollbackPending) == "interrupted validation evidence");
    fs::remove(rollbackPending);
    size_t failedHistoryProbes = 0;
    bool historyNotRecovered = false;
    try {
        ccode::ValidateProfileRollback(activationRoot, historyId, snapshotId, incompatibleEngine, testDigest,
            [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                ++failedHistoryProbes; return false;
            });
    } catch (const std::runtime_error& error) {
        historyNotRecovered = std::string(error.what()) == "E_CANDIDATE_HISTORY";
    }
    assert(historyNotRecovered && failedHistoryProbes == 1);
    assert(!fs::exists(activationRoot / "rollback-candidates" / historyId / "rollback-validation.json"));
    for (const auto& changedPath : {activationRoot / "active-profile.json",
        activationRoot / "rollback-candidates" / historyId / "rollback.json"}) {
        const auto original = Read(changedPath);
        auto changed = ccode::Json::parse(original);
        changed["changed-during-probe"] = true;
        size_t probes = 0;
        bool changedBinding = false;
        try {
            ccode::ValidateProfileRollback(activationRoot, historyId, snapshotId, incompatibleEngine, testDigest,
                [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                    ++probes; Write(changedPath, changed.dump()); return true;
                });
        } catch (const std::runtime_error& error) {
            changedBinding = std::string(error.what()) == "E_ROLLBACK_CHANGED";
        }
        assert(changedBinding && probes == 1);
        assert(!fs::exists(activationRoot / "rollback-candidates" / historyId / "validation.json"));
        assert(Read(changedPath) == changed.dump());
        Write(changedPath, original);
    }
    // A different prepared candidate isolates the failed publication evidence.
    const std::string lateRollbackId = "11111111-1111-4111-8111-111111111111";
    const std::string latePreservationId = "22222222-2222-4222-8222-222222222222";
    const std::string lateVerificationId = "33333333-3333-4333-8333-333333333333";
    ccode::PrepareProfileRollback(activationRoot, historyId, lateRollbackId,
        latePreservationId, incompatibleEngine, testDigest);
    const auto lateRollback = activationRoot / "rollback-candidates" / lateRollbackId;
    const auto lateRollbackPending = lateRollback / "rollback-validation.json.pending";
    bool lateRollbackRejected = false;
    size_t lateRollbackProbes = 0;
    try {
        ccode::ValidateProfileRollback(activationRoot, lateRollbackId, lateVerificationId,
            incompatibleEngine, testDigest,
            [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                if (++lateRollbackProbes == 1)
                    Write(lateRollbackPending, "retain interrupted rollback evidence");
                return true;
            });
    } catch (const std::runtime_error& error) {
        lateRollbackRejected = std::string(error.what()) == "E_ROLLBACK_WRITE";
    }
    assert(Read(lateRollbackPending) == "retain interrupted rollback evidence");
    assert(lateRollbackRejected && lateRollbackProbes == 2);
    assert(!fs::exists(lateRollback / "rollback-validation.json"));
    assert(fs::exists(lateRollback / "validation.json"));
    // A completed inner receipt alone cannot authorize the rollback.
    const auto retainedEvidence = ccode::CandidateFiles(lateRollback, testDigest);
    bool partialRollbackDenied = false;
    try { ccode::ActivateProfileRollback(activationRoot, lateRollbackId, incompatibleEngine, testDigest); }
    catch (const std::runtime_error& error) {
        partialRollbackDenied = std::string(error.what()) == "E_CANDIDATE_DATA";
    }
    assert(partialRollbackDenied);
    assert(ccode::CandidateFiles(lateRollback, testDigest) == retainedEvidence);
    assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    assert(ccode::CandidateFiles(nextCandidate / "profile", testDigest) == currentFiles);
    assert(ccode::VerifyProfileSnapshot(activationRoot / "snapshots" / latePreservationId,
        testDigest)["files"] == currentFiles);
    const std::string publishedRollbackId = "44444444-4444-4444-8444-444444444444";
    const std::string publishedPreservationId = "55555555-5555-4555-8555-555555555555";
    const std::string publishedVerificationId = "66666666-6666-4666-8666-666666666666";
    ccode::PrepareProfileRollback(activationRoot, historyId, publishedRollbackId,
        publishedPreservationId, incompatibleEngine, testDigest);
    const auto publishedRollback = activationRoot / "rollback-candidates" / publishedRollbackId;
    bool publishedRollbackRejected = false;
    size_t publishedRollbackProbes = 0;
    try {
        ccode::ValidateProfileRollback(activationRoot, publishedRollbackId, publishedVerificationId,
            incompatibleEngine, testDigest,
            [&](const fs::path&, const fs::path&, const std::string&, const std::string&) {
                if (++publishedRollbackProbes == 1)
                    Write(publishedRollback / "rollback-validation.json", "retain published rollback evidence");
                return true;
            });
    } catch (const std::runtime_error& error) {
        publishedRollbackRejected = std::string(error.what()) == "E_ROLLBACK_WRITE";
    }
    assert(Read(publishedRollback / "rollback-validation.json") == "retain published rollback evidence");
    assert(publishedRollbackRejected && publishedRollbackProbes == 2);
    assert(ccode::ReadCandidateDocument(publishedRollback / "rollback-validation.json.pending")
        ["validation"]["historyVerified"] == true);
    assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    assert(ccode::CandidateFiles(nextCandidate / "profile", testDigest) == currentFiles);
    size_t rollbackProbes = 0;
    const auto rollbackReceipt = ccode::ValidateProfileRollback(activationRoot, historyId,
        snapshotId, incompatibleEngine, testDigest,
        [&](const fs::path& isolated, const fs::path& workspace, const std::string& id,
            const std::string& expected) {
            ++rollbackProbes;
            assert(isolated == activationRoot / "rollback-candidates" / historyId / "profile");
            assert(ccode::CandidateHistoryText(isolated, workspace, id) == expected);
            assert(!expected.empty());
            return true;
        });
    assert(rollbackProbes == 2);
    assert(rollbackReceipt["plan"] == rollback);
    assert(rollbackReceipt["validation"]["scope"] == "all-top-level-sessions");
    assert(rollbackReceipt["validation"]["engine"] == incompatibleEngine);
    assert(ccode::ReadCandidateDocument(activationRoot / "rollback-candidates" / historyId /
        "rollback-validation.json") == rollbackReceipt);
    assert(ccode::CandidateFiles(nextCandidate / "profile", testDigest) == currentFiles);
    assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    const auto rollbackProfile = activationRoot / "rollback-candidates" / historyId / "profile";
    Write(nextCandidate / "profile/after-verification.txt", "new data since validation");
    bool staleRollbackCommit = false;
    try { ccode::ActivateProfileRollback(activationRoot, historyId, incompatibleEngine, testDigest); }
    catch (const std::runtime_error& error) {
        staleRollbackCommit = std::string(error.what()) == "E_ROLLBACK_CHANGED";
    }
    assert(staleRollbackCommit);
    assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    assert(!fs::exists(activationRoot / "active-profile.json.pending"));
    assert(Read(nextCandidate / "profile/after-verification.txt") == "new data since validation");
    fs::remove(nextCandidate / "profile/after-verification.txt");
    const auto rollbackEvidencePath = rollbackProfile.parent_path() / "rollback-validation.json";
    auto wrongBinding = rollbackReceipt;
    wrongBinding["plan"]["sourceSnapshotId"] = secondId;
    Write(rollbackEvidencePath, wrongBinding.dump());
    bool mismatchedRollbackReceipt = false;
    try { ccode::ActivateProfileRollback(activationRoot, historyId, incompatibleEngine, testDigest); }
    catch (const std::runtime_error& error) {
        mismatchedRollbackReceipt = std::string(error.what()) == "E_ROLLBACK_DATA";
    }
    assert(mismatchedRollbackReceipt);
    assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    Write(rollbackEvidencePath, rollbackReceipt.dump());
    const auto oldManifestPath = activationRoot / "snapshots" / historyId / "manifest.json";
    const auto oldManifestBytes = Read(oldManifestPath);
    Write(oldManifestPath, "{}");
    bool damagedRollbackSource = false;
    try { ccode::ActivateProfileRollback(activationRoot, historyId, incompatibleEngine, testDigest); }
    catch (const std::runtime_error& error) {
        damagedRollbackSource = std::string(error.what()) == "E_SNAPSHOT_DATA";
    }
    assert(damagedRollbackSource);
    assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    assert(!fs::exists(activationRoot / "active-profile.json.pending"));
    Write(oldManifestPath, oldManifestBytes);
    Write(activationRoot / "active-profile.json.pending", "interrupted rollback evidence");
    bool pendingRollbackCommit = false;
    try { ccode::ActivateProfileRollback(activationRoot, historyId, incompatibleEngine, testDigest); }
    catch (const std::runtime_error& error) {
        pendingRollbackCommit = std::string(error.what()) == "E_ACTIVATION_PENDING";
    }
    assert(pendingRollbackCommit);
    assert(Read(activationRoot / "active-profile.json") == secondPointerBytes);
    assert(Read(activationRoot / "active-profile.json.pending") == "interrupted rollback evidence");
    ccode::ArchiveActivationPending(activationRoot, preservationId);
    ccode::ActivateProfileRollback(activationRoot, historyId, incompatibleEngine, testDigest);
    assert(ccode::ResolveActiveProfile(activationRoot, incompatibleEngine) == rollbackProfile);
    assert(ccode::ResolveProfileForBackup(activationRoot) == rollbackProfile);
    assert(ccode::CandidateFiles(nextCandidate / "profile", testDigest) == currentFiles);
    assert(ccode::VerifyProfileSnapshot(activationRoot / "snapshots" / preservationId,
        testDigest)["files"] == currentFiles);
    const auto rollbackPointer = ccode::Json::parse(Read(activationRoot / "active-profile.json"));
    assert(rollbackPointer["schema"] == 2); // Old launchers must reject, never select ordinary candidates.
    assert(rollbackPointer["profileKind"] == "rollback");
    assert(rollbackPointer["preservationSnapshotId"] == preservationId);
    assert(rollbackPointer["engine"] == incompatibleEngine);
    bool rollbackAlreadyActive = false;
    try { ccode::ActivateProfileRollback(activationRoot, historyId, incompatibleEngine, testDigest); }
    catch (const std::runtime_error& error) {
        rollbackAlreadyActive = std::string(error.what()) == "E_ACTIVATION_ALREADY_ACTIVE";
    }
    assert(rollbackAlreadyActive);
    const auto committedRollbackBytes = Read(activationRoot / "active-profile.json");
    Write(rollbackProfile / "new-runtime-data.txt", "normal writes after rollback");
    assert(ccode::ResolveActiveProfile(activationRoot, incompatibleEngine) == rollbackProfile);
    for (const auto* key : {"profileKind", "preservationSnapshotId"}) {
        auto invalidState = rollbackPointer;
        invalidState[key] = "invalid";
        Write(activationRoot / "active-profile.json", invalidState.dump());
        bool invalidRollbackPointer = false;
        try { ccode::ResolveProfileForBackup(activationRoot); }
        catch (const std::runtime_error& error) {
            invalidRollbackPointer = std::string(error.what()) == "E_ACTIVE_PROFILE";
        }
        assert(invalidRollbackPointer);
        Write(activationRoot / "active-profile.json", committedRollbackBytes);
    }
    Write(rollbackEvidencePath, wrongBinding.dump());
    bool inconsistentRollbackBinding = false;
    try { ccode::ResolveActiveProfile(activationRoot, incompatibleEngine); }
    catch (const std::runtime_error& error) {
        inconsistentRollbackBinding = std::string(error.what()) == "E_ACTIVE_PROFILE";
    }
    assert(inconsistentRollbackBinding);
    Write(rollbackEvidencePath, rollbackReceipt.dump());
    auto partialReceipt = allReceipt;
    partialReceipt["sessions"].erase(1);
    Write(allCandidate / "validation.json", partialReceipt.dump());
    bool omittedSession = false;
    try { ccode::VerifyCandidateValidation(allCandidate, root / "all-verified", engine, testDigest); }
    catch (const std::runtime_error& error) { omittedSession = std::string(error.what()) == "E_CANDIDATE_DATA"; }
    assert(omittedSession);
    Write(allCandidate / "validation.json", allReceipt.dump());
    const auto driftCandidate = ccode::StageProfileCandidate(allSnapshot, root / "all-candidates", secondId, testDigest);
    bool inventoryDrift = false;
    try {
        ccode::ValidateProfileCandidate(driftCandidate, allSnapshot, inventoryProfile,
            root / "drift-verified", snapshotId, root, "", engine, testDigest, {},
            [&](const fs::path& isolated, const fs::path&, const std::string& id, const std::string&) {
                if (id == historyId) {
                    auto changed = ccode::Json::parse(secondSaved);
                    changed["message"]["content"] = "replaced original marker";
                    Write(isolated / "home/.claude/projects/second" / (secondId + ".jsonl"), changed.dump() + "\n");
                }
                return true;
            });
    } catch (const std::runtime_error& error) { inventoryDrift = std::string(error.what()) == "E_CANDIDATE_CHANGED"; }
    assert(inventoryDrift && !fs::exists(driftCandidate / "validation.json"));
    fs::remove_all(ccode::SnapshotIoPath(root));
    std::cout << "ccode profile recovery tests passed\n";
} catch (const std::exception& error) {
    std::cerr << "profile test exception: " << error.what() << '\n';
    return 1;
}
