#pragma once
#include "snapshot.hpp"
#include "workspaces.hpp"
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ccode {
// This is local integrity evidence, not an authenticated security boundary.
// Callers hold both the active-profile and candidate-profile exclusive locks.
using CandidateProbe = std::function<bool(const std::filesystem::path&, const std::string&, const std::string&)>;
using CandidateWorkspaceProbe = std::function<bool(const std::filesystem::path&, const std::filesystem::path&,
    const std::string&, const std::string&)>;
// Exclusively allocate receipt evidence. Never truncate an interrupted writer's
// file; retain our own partial file on write failure for explicit recovery.
inline void WriteCandidateEvidence(const std::filesystem::path& destination,
    const std::string& bytes, const char* error) {
    const auto path = SnapshotIoPath(destination);
#ifdef _WIN32
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error(error);
    DWORD written = 0;
    const bool saved = bytes.size() <= MAXDWORD &&
        WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size() && FlushFileBuffers(file);
    const bool closed = CloseHandle(file) != 0;
    if (!saved || !closed) throw std::runtime_error(error);
#else
    const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) throw std::runtime_error(error);
    const bool saved = ::write(file, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size()) && ::fsync(file) == 0;
    const int closed = ::close(file);
    if (!saved || closed != 0) throw std::runtime_error(error);
#endif
}
// Publish without replacing evidence created after the caller's preflight.
// Both paths are in the same directory. This is not protection against a
// hostile process replacing our pending path, nor a directory-fsync guarantee.
inline void PublishCandidateEvidence(const std::filesystem::path& pendingPath,
    const std::filesystem::path& destination, const char* error) {
    const auto pending = SnapshotIoPath(pendingPath);
    const auto target = SnapshotIoPath(destination);
#ifdef _WIN32
    if (!MoveFileExW(pending.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error(error);
#else
    // link atomically allocates a new directory entry and fails if it exists.
    // Keep pending on publication failure; never fall back to replacing rename.
    if (::link(pending.c_str(), target.c_str()) != 0) throw std::runtime_error(error);
    if (::unlink(pending.c_str()) != 0) throw std::runtime_error(error);
#endif
}
inline Json ReadCandidateDocument(const std::filesystem::path& file) {
    namespace fs = std::filesystem;
    const auto path = SnapshotIoPath(file);
    if (fs::is_symlink(fs::symlink_status(path))) throw std::runtime_error("E_CANDIDATE_DATA");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("E_CANDIDATE_DATA");
    const auto value = Json::parse(input, nullptr, false);
    if (!value.is_object()) throw std::runtime_error("E_CANDIDATE_DATA");
    return value;
}
inline Json CandidateFiles(const std::filesystem::path& directory, const SnapshotDigest& digest) {
    namespace fs = std::filesystem;
    const auto profile = SnapshotIoPath(directory);
    if (fs::is_symlink(fs::symlink_status(profile))) throw std::runtime_error("E_CANDIDATE_DATA");
    Json files = Json::object();
    for (const auto& entry : fs::recursive_directory_iterator(profile)) {
        const auto name = entry.path().lexically_relative(profile).generic_u8string();
        if (entry.is_symlink()) throw std::runtime_error("E_CANDIDATE_DATA");
        if (name == "frontend.lock") continue;
        if (entry.is_directory()) continue;
        if (!entry.is_regular_file()) throw std::runtime_error("E_CANDIDATE_DATA");
        files[name] = {{"sha256", digest(entry.path())}, {"size", entry.file_size()}};
    }
    return files;
}
inline std::string CandidateHistoryText(const std::filesystem::path& profile,
    const std::filesystem::path& workspace, const std::string& session) {
    namespace fs = std::filesystem;
    if (!ValidSessionId(session)) throw std::runtime_error("E_SESSION_ID");
    const auto projects = SnapshotIoPath(profile / "home/.claude/projects");
    if (!fs::is_directory(projects)) throw std::runtime_error("E_CANDIDATE_HISTORY");
    std::string result;
    size_t matches = 0;
    for (const auto& project : fs::directory_iterator(projects)) {
        if (project.is_symlink() || !project.is_directory()) continue;
        const auto transcript = project.path() / (session + ".jsonl");
        if (fs::is_symlink(fs::symlink_status(transcript))) throw std::runtime_error("E_CANDIDATE_HISTORY");
        std::ifstream input(transcript, std::ios::binary);
        if (!input) continue;
        std::string line;
        while (std::getline(input, line)) {
            if (line.size() > 16 * 1024 * 1024) throw std::runtime_error("E_CANDIDATE_HISTORY");
            const auto event = Json::parse(line, nullptr, false);
            if (!event.is_object()) throw std::runtime_error("E_CANDIDATE_HISTORY");
            if (!event.contains("type") || event["type"] != "user") continue;
            if (!event.contains("sessionId") || event["sessionId"] != session ||
                !event.contains("cwd") || !event["cwd"].is_string() ||
                !SameWorkspace(fs::u8path(event["cwd"].get<std::string>()), workspace)) continue;
            if (event.contains("isSidechain") && event["isSidechain"] != false) continue;
            if (!event.contains("message") || !event["message"].is_object() ||
                !event["message"].contains("content")) throw std::runtime_error("E_CANDIDATE_HISTORY");
            const auto& content = event["message"]["content"];
            std::string text;
            if (content.is_string()) text = content.get<std::string>();
            else if (content.is_array()) {
                for (const auto& block : content) {
                    if (!block.is_object() || !block.contains("type") || block["type"] != "text" ||
                        !block.contains("text") || !block["text"].is_string())
                        throw std::runtime_error("E_CANDIDATE_HISTORY");
                    text += block["text"].get<std::string>();
                }
            }
            if (text.empty() || text.size() > 65536 || ConsoleText(text) != text)
                throw std::runtime_error("E_CANDIDATE_HISTORY");
            result = text;
            ++matches;
            break;
        }
    }
    if (matches != 1) throw std::runtime_error("E_CANDIDATE_HISTORY");
    return result;
}
struct CandidateSession {
    std::string session;
    std::filesystem::path workspace;
    std::string expected;
};
// Enumerate authoritative top-level transcripts across every native project.
// Nested subagent transcripts are preserved, not claimed as independently resumed.
inline std::vector<CandidateSession> CandidateSessionInventory(const std::filesystem::path& profile) {
    namespace fs = std::filesystem;
    const auto projects = SnapshotIoPath(profile / "home/.claude/projects");
    if (!fs::is_directory(projects) || fs::is_symlink(fs::symlink_status(projects)))
        throw std::runtime_error("E_CANDIDATE_HISTORY");
    std::vector<CandidateSession> result;
    std::set<std::string> identities;
    for (const auto& project : fs::directory_iterator(projects)) {
        if (project.is_symlink()) throw std::runtime_error("E_CANDIDATE_HISTORY");
        if (!project.is_directory()) continue;
        for (const auto& file : fs::directory_iterator(project.path())) {
            if (file.path().extension() != ".jsonl") continue;
            if (file.is_symlink() || !file.is_regular_file()) throw std::runtime_error("E_CANDIDATE_HISTORY");
            const auto id = file.path().stem().u8string();
            if (!ValidSessionId(id) || !identities.insert(id).second) throw std::runtime_error("E_CANDIDATE_HISTORY");
            std::ifstream input(file.path(), std::ios::binary);
            if (!input) throw std::runtime_error("E_CANDIDATE_HISTORY");
            fs::path workspace;
            std::string line;
            while (std::getline(input, line)) {
                if (line.size() > 16 * 1024 * 1024) throw std::runtime_error("E_CANDIDATE_HISTORY");
                const auto event = Json::parse(line, nullptr, false);
                if (!event.is_object()) throw std::runtime_error("E_CANDIDATE_HISTORY");
                if (!event.contains("type") || event["type"] != "user") continue;
                if (event.contains("isSidechain") && !event["isSidechain"].is_boolean())
                    throw std::runtime_error("E_CANDIDATE_HISTORY");
                if (event.value("isSidechain", false)) continue;
                if (!event.contains("sessionId") || event["sessionId"] != id ||
                    !event.contains("cwd") || !event["cwd"].is_string()) throw std::runtime_error("E_CANDIDATE_HISTORY");
                const auto cwd = fs::u8path(event["cwd"].get<std::string>());
                if (!cwd.is_absolute() || !fs::is_directory(cwd)) throw std::runtime_error("E_CANDIDATE_WORKSPACE");
                if (!workspace.empty() && !SameWorkspace(workspace, cwd)) throw std::runtime_error("E_CANDIDATE_HISTORY");
                workspace = cwd;
            }
            if (input.bad() || workspace.empty()) throw std::runtime_error("E_CANDIDATE_HISTORY");
            // Preflight every marker before any engine request starts.
            const auto expected = CandidateHistoryText(profile, workspace, id);
            result.push_back({id, workspace, expected});
        }
    }
    if (result.empty()) throw std::runtime_error("E_CANDIDATE_HISTORY");
    std::sort(result.begin(), result.end(), [](const CandidateSession& a, const CandidateSession& b) {
        return a.session < b.session;
    });
    return result;
}
inline bool ValidCandidateEngine(const Json& engine) {
    if (!engine.is_object() || !engine.contains("version") || !engine["version"].is_string() ||
        engine["version"].get<std::string>().empty() || !engine.contains("sha256") || !engine["sha256"].is_string()) return false;
    const auto hash = engine["sha256"].get<std::string>();
    return hash.size() == 64 && hash.find_first_not_of("0123456789abcdef") == std::string::npos;
}
inline Json ValidateProfileCandidate(const std::filesystem::path& candidatePath,
    const std::filesystem::path& sourceSnapshot, const std::filesystem::path& activeProfile,
    const std::filesystem::path& verifiedRoot,
    const std::string& verificationId, const std::filesystem::path& workspace,
    const std::string& session, const Json& engine, const SnapshotDigest& digest, const CandidateProbe& probe,
    const CandidateWorkspaceProbe& allProbe = {}) {
    namespace fs = std::filesystem;
    const auto candidate = SnapshotIoPath(candidatePath);
    if (!ValidCandidateEngine(engine) || !ValidSessionId(verificationId)) throw std::runtime_error("E_CANDIDATE_DATA");
    if (fs::exists(fs::symlink_status(candidate / "validation.json")) ||
        fs::exists(fs::symlink_status(candidate / "validation.json.pending"))) throw std::runtime_error("E_CANDIDATE_EXISTS");
    const auto metadata = ReadCandidateDocument(candidate / "candidate.json");
    if (!metadata.contains("schema") || metadata["schema"] != 1 ||
        !metadata.contains("state") || metadata["state"] != "staged" ||
        !metadata.contains("candidateId") || metadata["candidateId"] != candidate.filename().u8string() ||
        !metadata.contains("sourceSnapshotId") || metadata["sourceSnapshotId"] != sourceSnapshot.filename().u8string())
        throw std::runtime_error("E_CANDIDATE_DATA");
    const auto source = VerifyProfileSnapshot(sourceSnapshot, digest);
    if (CandidateFiles(activeProfile, digest) != source["files"]) throw std::runtime_error("E_SOURCE_CHANGED");
    // A live candidate has frontend.lock while an immutable snapshot must not.
    // Validate the full expected manifest and all non-lock files without relaxing
    // VerifyProfileSnapshot's strict rejection of extra backup files.
    auto expectedManifest = source;
    expectedManifest["snapshotId"] = metadata["candidateId"];
    if (ReadCandidateDocument(candidate / "manifest.json") != expectedManifest ||
        CandidateFiles(candidate / "profile", digest) != source["files"])
        throw std::runtime_error("E_CANDIDATE_CHANGED");
    if ((allProbe && !session.empty()) || (!allProbe && !probe)) throw std::runtime_error("E_ARGUMENT");
    const auto targets = allProbe ? CandidateSessionInventory(candidate / "profile") :
        std::vector<CandidateSession>{{session, workspace, CandidateHistoryText(candidate / "profile", workspace, session)}};
    Json sessions = Json::array();
    for (const auto& target : targets) {
        const auto& expected = target.expected;
        if (CandidateHistoryText(candidate / "profile", target.workspace, target.session) != expected)
            throw std::runtime_error("E_CANDIDATE_CHANGED");
        const bool restored = allProbe ? allProbe(candidatePath / "profile", target.workspace, target.session, expected) :
            probe(candidatePath / "profile", target.session, expected);
        if (!restored) throw std::runtime_error("E_CANDIDATE_HISTORY");
    }
    if (VerifyProfileSnapshot(sourceSnapshot, digest) != source ||
        ReadCandidateDocument(candidate / "candidate.json") != metadata)
        throw std::runtime_error("E_CANDIDATE_CHANGED");
    if (CandidateFiles(activeProfile, digest) != source["files"]) throw std::runtime_error("E_SOURCE_CHANGED");
    if (allProbe) {
        const auto after = CandidateSessionInventory(candidate / "profile");
        if (after.size() != targets.size()) throw std::runtime_error("E_CANDIDATE_CHANGED");
        for (size_t i = 0; i < targets.size(); ++i)
            if (after[i].session != targets[i].session || after[i].expected != targets[i].expected ||
                !SameWorkspace(after[i].workspace, targets[i].workspace)) throw std::runtime_error("E_CANDIDATE_CHANGED");
    }
    for (const auto& target : targets) {
        const auto id = ResolveWorkspace(SnapshotIoPath(candidate / "profile/workspaces.json"), target.workspace);
        sessions.push_back({{"sessionId", target.session}, {"workspaceId", id}});
    }
    const auto checkpoint = CreateProfileSnapshot(candidate / "profile", verifiedRoot, verificationId, digest);
    const auto frozen = VerifyProfileSnapshot(checkpoint, digest);
    if (CandidateFiles(candidate / "profile", digest) != frozen["files"]) throw std::runtime_error("E_CANDIDATE_CHANGED");
    Json receipt = {{"schema", 1}, {"candidateId", metadata["candidateId"]},
        {"sourceSnapshotId", source["snapshotId"]}, {"verificationId", verificationId},
        {"scope", allProbe ? "all-top-level-sessions" : "single-session"},
        {"engine", engine}, {"adapter", "stream-json-v1"}, {"historyVerified", true},
        {"files", frozen["files"]}};
    if (allProbe) receipt["sessions"] = sessions;
    else {
        receipt["sessionId"] = sessions[0]["sessionId"];
        receipt["workspaceId"] = sessions[0]["workspaceId"];
    }
    const auto pending = candidate / "validation.json.pending";
    WriteCandidateEvidence(pending, receipt.dump(2) + "\n", "E_CANDIDATE_WRITE");
    PublishCandidateEvidence(pending, candidate / "validation.json", "E_CANDIDATE_WRITE");
    return receipt;
}
inline Json VerifyCandidateValidation(const std::filesystem::path& candidate,
    const std::filesystem::path& verifiedRoot, const Json& engine, const SnapshotDigest& digest) {
    const auto receipt = ReadCandidateDocument(candidate / "validation.json");
    if (!receipt.contains("schema") || receipt["schema"] != 1 ||
        !receipt.contains("candidateId") || receipt["candidateId"] != candidate.filename().u8string() ||
        !receipt.contains("verificationId") || !receipt["verificationId"].is_string() ||
        !ValidSessionId(receipt["verificationId"].get<std::string>()) ||
        !receipt.contains("scope") || (receipt["scope"] != "single-session" && receipt["scope"] != "all-top-level-sessions") ||
        !receipt.contains("historyVerified") || receipt["historyVerified"] != true ||
        !receipt.contains("adapter") || receipt["adapter"] != "stream-json-v1" ||
        !receipt.contains("engine") || !ValidCandidateEngine(engine) || receipt["engine"] != engine ||
        !receipt.contains("files")) throw std::runtime_error("E_CANDIDATE_DATA");
    const auto validIdentity = [](const Json& item) {
        return item.is_object() && item.contains("sessionId") && item["sessionId"].is_string() &&
            ValidSessionId(item["sessionId"].get<std::string>()) && item.contains("workspaceId") &&
            item["workspaceId"].is_string() && ValidSessionId(item["workspaceId"].get<std::string>());
    };
    if (receipt["scope"] == "single-session") {
        if (!validIdentity(receipt)) throw std::runtime_error("E_CANDIDATE_DATA");
    } else {
        if (!receipt.contains("sessions") || !receipt["sessions"].is_array() || receipt["sessions"].empty() ||
            receipt.contains("sessionId") || receipt.contains("workspaceId")) throw std::runtime_error("E_CANDIDATE_DATA");
        for (const auto& item : receipt["sessions"])
            if (!validIdentity(item)) throw std::runtime_error("E_CANDIDATE_DATA");
    }
    const auto metadata = ReadCandidateDocument(candidate / "candidate.json");
    if (!metadata.contains("schema") || metadata["schema"] != 1 ||
        !metadata.contains("state") || metadata["state"] != "staged" ||
        !receipt.contains("sourceSnapshotId") || !receipt["sourceSnapshotId"].is_string() ||
        !ValidSessionId(receipt["sourceSnapshotId"].get<std::string>()) ||
        !metadata.contains("sourceSnapshotId") || receipt["sourceSnapshotId"] != metadata["sourceSnapshotId"] ||
        !metadata.contains("candidateId") || receipt["candidateId"] != metadata["candidateId"])
        throw std::runtime_error("E_CANDIDATE_DATA");
    const auto frozen = VerifyProfileSnapshot(verifiedRoot / receipt["verificationId"].get<std::string>(), digest);
    if (receipt["files"] != frozen["files"] || CandidateFiles(candidate / "profile", digest) != frozen["files"])
        throw std::runtime_error("E_CANDIDATE_CHANGED");
    if (receipt["scope"] == "all-top-level-sessions") {
        const auto targets = CandidateSessionInventory(candidate / "profile");
        const auto registry = ReadCandidateDocument(candidate / "profile/workspaces.json");
        if (!registry.contains("schema") || registry["schema"] != 1 ||
            !registry.contains("workspaces") || !registry["workspaces"].is_object()) throw std::runtime_error("E_CANDIDATE_DATA");
        Json expected = Json::array();
        for (const auto& target : targets) {
            const auto key = WorkspaceKey(target.workspace);
            if (!registry["workspaces"].contains(key)) throw std::runtime_error("E_CANDIDATE_DATA");
            expected.push_back({{"sessionId", target.session}, {"workspaceId", registry["workspaces"][key]}});
        }
        if (receipt["sessions"] != expected) throw std::runtime_error("E_CANDIDATE_DATA");
    }
    return receipt;
}
// Pre-activation gate only; caller must hold the data-root coordination lock and
// both profile locks through the eventual atomic pointer commit.
inline Json VerifyCandidateActivation(const std::filesystem::path& candidate,
    const std::filesystem::path& sourceSnapshot, const std::filesystem::path& activeProfile,
    const std::filesystem::path& verifiedRoot, const Json& engine, const SnapshotDigest& digest) {
    const auto receipt = VerifyCandidateValidation(candidate, verifiedRoot, engine, digest);
    if (receipt["scope"] != "all-top-level-sessions") throw std::runtime_error("E_ACTIVATION_SCOPE");
    const auto source = VerifyProfileSnapshot(sourceSnapshot, digest);
    if (source["snapshotId"] != receipt["sourceSnapshotId"]) throw std::runtime_error("E_CANDIDATE_DATA");
    if (CandidateFiles(activeProfile, digest) != source["files"]) throw std::runtime_error("E_SOURCE_CHANGED");
    return receipt;
}

// Selection is read under the data-root coordination lock. A missing pointer
// preserves legacy layout; an existing invalid pointer must never fall back.
// Only for byte-preserving backup: this does NOT authorize engine execution.
inline std::filesystem::path ResolveProfileForBackup(const std::filesystem::path& data) {
    namespace fs = std::filesystem;
    const auto pointer = SnapshotIoPath(data / "active-profile.json");
    if (!fs::exists(fs::symlink_status(pointer))) return data / "profile";
    try {
        const auto state = ReadCandidateDocument(pointer);
        if (!state.is_object() || (state.value("schema", 0) != 1 && state.value("schema", 0) != 2) ||
            !state.contains("engine") || !ValidCandidateEngine(state["engine"]) ||
            state.value("adapter", "") != "stream-json-v1") throw std::runtime_error("invalid");
        for (const auto* key : {"candidateId", "verificationId", "sourceSnapshotId"})
            if (!state.contains(key) || !state[key].is_string() ||
                !ValidSessionId(state[key].get<std::string>())) throw std::runtime_error("invalid");
        const auto kind = state.value("profileKind", "candidate");
        if ((kind != "candidate" && kind != "rollback") ||
            state["schema"] != (kind == "rollback" ? 2 : 1)) throw std::runtime_error("invalid");
        const auto candidateRoot = data / (kind == "rollback" ? "rollback-candidates" : "candidates");
        const auto candidate = candidateRoot / state["candidateId"].get<std::string>();
        for (const auto& directory : {candidateRoot, candidate, candidate / "profile"}) {
            const auto status = fs::symlink_status(SnapshotIoPath(directory));
            if (fs::is_symlink(status) || !fs::is_directory(status)) throw std::runtime_error("invalid");
        }
        const auto metadata = ReadCandidateDocument(candidate / "candidate.json");
        const auto receipt = ReadCandidateDocument(candidate / "validation.json");
        if (metadata.value("schema", 0) != 1 || metadata.value("state", "") != "staged" ||
            receipt.value("schema", 0) != 1 || receipt.value("scope", "") != "all-top-level-sessions" ||
            !receipt.contains("historyVerified") || receipt["historyVerified"] != true)
            throw std::runtime_error("invalid");
        for (const auto* key : {"candidateId", "sourceSnapshotId"})
            if (!metadata.contains(key) || metadata[key] != state[key]) throw std::runtime_error("invalid");
        for (const auto* key : {"candidateId", "sourceSnapshotId", "verificationId", "engine", "adapter"})
            if (!receipt.contains(key) || receipt[key] != state[key]) throw std::runtime_error("invalid");
        if (kind == "rollback") {
            const auto evidence = ReadCandidateDocument(candidate / "rollback-validation.json");
            const auto plan = ReadCandidateDocument(candidate / "rollback.json");
            if (evidence.value("schema", 0) != 1 || !evidence.contains("plan") || evidence["plan"] != plan ||
                !evidence.contains("validation") || evidence["validation"] != receipt ||
                !state.contains("preservationSnapshotId") || !state["preservationSnapshotId"].is_string() ||
                !ValidSessionId(state["preservationSnapshotId"].get<std::string>()) ||
                plan.value("schema", 0) != 1 || plan.value("state", "") != "prepared" ||
                plan.value("adapter", "") != "stream-json-v1" ||
                !plan.contains("targetEngine") || plan["targetEngine"] != state["engine"])
                throw std::runtime_error("invalid");
            for (const auto* key : {"candidateId", "sourceSnapshotId", "preservationSnapshotId"})
                if (!plan.contains(key) || plan[key] != state[key]) throw std::runtime_error("invalid");
        }
        return candidate / "profile";
    } catch (const std::exception&) {
        throw std::runtime_error("E_ACTIVE_PROFILE");
    }
}

inline void VerifyRollbackDirectory(const std::filesystem::path& data, const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    for (auto parent = fs::absolute(path); !parent.empty();) {
        const auto status = fs::symlink_status(SnapshotIoPath(parent));
        if (fs::is_symlink(status) || (fs::exists(status) && !fs::is_directory(status)))
            throw std::runtime_error("E_ROLLBACK_PATH");
        if (parent == fs::absolute(data)) break;
        const auto next = parent.parent_path();
        if (next == parent) break;
        parent = next;
    }
}

// Caller holds the data-root and current profile locks for the whole operation.
// Preparation is not compatibility approval and never changes the active pointer.
inline Json PrepareProfileRollback(const std::filesystem::path& data,
    const std::string& sourceId, const std::string& rollbackId,
    const std::string& preservationId, const Json& targetEngine, const SnapshotDigest& digest) {
    namespace fs = std::filesystem;
    if (!ValidSessionId(sourceId) || !ValidSessionId(rollbackId) ||
        !ValidSessionId(preservationId) || !ValidCandidateEngine(targetEngine))
        throw std::runtime_error("E_ROLLBACK_DATA");
    // Reject linked ancestors before any snapshot or staging writes.
    for (const auto& path : {data / "snapshots" / sourceId,
        data / "rollback-candidates" / ".staging"}) {
        VerifyRollbackDirectory(data, path);
    }
    for (const auto& destination : {data / "snapshots" / preservationId,
        data / "rollback-candidates" / rollbackId,
        data / "rollback-candidates" / ".staging" / rollbackId})
        if (fs::exists(fs::symlink_status(SnapshotIoPath(destination))))
            throw std::runtime_error("E_ROLLBACK_EXISTS");
    const auto source = data / "snapshots" / sourceId;
    const auto current = ResolveProfileForBackup(data);
    const auto pointerPath = SnapshotIoPath(data / "active-profile.json");
    const auto prior = fs::exists(pointerPath) ? ReadCandidateDocument(pointerPath) : Json(nullptr);
    const auto selected = VerifyProfileSnapshot(source, digest);
    const auto preserved = CreateProfileSnapshot(current, data / "snapshots", preservationId, digest);
    const auto candidate = StageProfileCandidate(source, data / "rollback-candidates", rollbackId, digest);
    if (CandidateFiles(current, digest) != VerifyProfileSnapshot(preserved, digest)["files"] ||
        VerifyProfileSnapshot(source, digest) != selected ||
        (fs::exists(pointerPath) ? ReadCandidateDocument(pointerPath) : Json(nullptr)) != prior)
        throw std::runtime_error("E_ROLLBACK_CHANGED");
    const Json plan = {{"schema", 1}, {"state", "prepared"}, {"candidateId", rollbackId},
        {"sourceSnapshotId", sourceId}, {"preservationSnapshotId", preservationId},
        {"priorActivePointer", prior}, {"targetEngine", targetEngine}, {"adapter", "stream-json-v1"}};
    std::ofstream output(SnapshotIoPath(candidate / "rollback.json"), std::ios::binary);
    output << plan.dump(2) << '\n';
    output.close();
    if (!output) throw std::runtime_error("E_ROLLBACK_WRITE");
    return plan;
}

// Caller holds data-root, active-profile and rollback-candidate locks.
inline Json ValidateProfileRollback(const std::filesystem::path& data, const std::string& id,
    const std::string& verificationId, const Json& engine, const SnapshotDigest& digest,
    const CandidateWorkspaceProbe& probe) {
    namespace fs = std::filesystem;
    if (!ValidSessionId(id) || !probe) throw std::runtime_error("E_ROLLBACK_DATA");
    const auto candidate = data / "rollback-candidates" / id;
    VerifyRollbackDirectory(data, candidate / "profile");
    VerifyRollbackDirectory(data, data / "rollback-verified");
    for (const auto* name : {"rollback-validation.json", "rollback-validation.json.pending"})
        if (fs::exists(fs::symlink_status(SnapshotIoPath(candidate / name))))
            throw std::runtime_error("E_ROLLBACK_EXISTS");
    const auto plan = ReadCandidateDocument(candidate / "rollback.json");
    if (plan.value("schema", 0) != 1 || plan.value("state", "") != "prepared" ||
        plan.value("candidateId", "") != id || plan.value("adapter", "") != "stream-json-v1" ||
        !ValidCandidateEngine(engine) || !plan.contains("targetEngine") || plan["targetEngine"] != engine ||
        !plan.contains("priorActivePointer")) throw std::runtime_error("E_ROLLBACK_DATA");
    for (const auto* key : {"sourceSnapshotId", "preservationSnapshotId"})
        if (!plan.contains(key) || !plan[key].is_string() || !ValidSessionId(plan[key].get<std::string>()))
            throw std::runtime_error("E_ROLLBACK_DATA");
    const auto source = data / "snapshots" / plan.at("sourceSnapshotId").get<std::string>();
    const auto preservation = data / "snapshots" / plan["preservationSnapshotId"].get<std::string>();
    VerifyRollbackDirectory(data, source / "profile");
    VerifyRollbackDirectory(data, preservation / "profile");
    const auto preserved = VerifyProfileSnapshot(preservation, digest);
    const auto selected = VerifyProfileSnapshot(source, digest);
    const auto guard = [&]() {
        VerifyRollbackDirectory(data, candidate / "profile");
        VerifyRollbackDirectory(data, source / "profile");
        VerifyRollbackDirectory(data, preservation / "profile");
        const auto pointer = SnapshotIoPath(data / "active-profile.json");
        const auto state = fs::exists(fs::symlink_status(pointer)) ? ReadCandidateDocument(pointer) : Json(nullptr);
        if (state != plan["priorActivePointer"] ||
            ReadCandidateDocument(candidate / "rollback.json") != plan ||
            VerifyProfileSnapshot(preservation, digest) != preserved ||
            VerifyProfileSnapshot(source, digest) != selected ||
            CandidateFiles(ResolveProfileForBackup(data), digest) != preserved["files"])
            throw std::runtime_error("E_ROLLBACK_CHANGED");
    };
    guard();
    const auto receipt = ValidateProfileCandidate(candidate, source, source / "profile",
        data / "rollback-verified", verificationId, {}, "", engine, digest, {},
        [&](const fs::path& isolated, const fs::path& workspace, const std::string& session,
            const std::string& expected) {
            guard();
            const auto restored = probe(isolated, workspace, session, expected);
            guard();
            return restored;
        });
    guard();
    const Json result = {{"schema", 1}, {"plan", plan}, {"validation", receipt}};
    const auto pending = SnapshotIoPath(candidate / "rollback-validation.json.pending");
    WriteCandidateEvidence(pending, result.dump(2) + "\n", "E_ROLLBACK_WRITE");
    guard();
    PublishCandidateEvidence(pending, candidate / "rollback-validation.json", "E_ROLLBACK_WRITE");
    return result;
}

// Runtime selection still requires exactly the engine bound to the pointer.
inline std::filesystem::path ResolveActiveProfile(const std::filesystem::path& data, const Json& engine) {
    const auto profile = ResolveProfileForBackup(data);
    const auto pointer = SnapshotIoPath(data / "active-profile.json");
    if (std::filesystem::exists(std::filesystem::symlink_status(pointer))) {
        const auto state = ReadCandidateDocument(pointer);
        if (!ValidCandidateEngine(engine) || state["engine"] != engine)
            throw std::runtime_error("E_ACTIVE_PROFILE");
    }
    return profile;
}

// Caller holds the data-root coordination lock. Archive opaque interrupted
// bytes, never interpret them as an instruction to activate a profile.
inline std::filesystem::path ArchiveActivationPending(const std::filesystem::path& data,
    const std::string& recoveryId) {
    namespace fs = std::filesystem;
    if (!ValidSessionId(recoveryId)) throw std::runtime_error("E_ACTIVATION_RECOVERY_ID");
    const auto pending = SnapshotIoPath(data / "active-profile.json.pending");
    const auto status = fs::symlink_status(pending);
    if (!fs::exists(status)) throw std::runtime_error("E_ACTIVATION_PENDING_MISSING");
    if (fs::is_symlink(status) || !fs::is_regular_file(status))
        throw std::runtime_error("E_ACTIVATION_PENDING_INVALID");
    const auto archive = data / "activation-recovery";
    const auto destination = archive / recoveryId;
    const auto archiveStatus = fs::symlink_status(SnapshotIoPath(archive));
    if (fs::exists(archiveStatus) && (fs::is_symlink(archiveStatus) || !fs::is_directory(archiveStatus)))
        throw std::runtime_error("E_ACTIVATION_RECOVERY_INVALID");
    if (fs::exists(fs::symlink_status(SnapshotIoPath(destination))))
        throw std::runtime_error("E_ACTIVATION_RECOVERY_EXISTS");
    try {
        fs::create_directories(SnapshotIoPath(archive));
        if (!fs::create_directory(SnapshotIoPath(destination)))
            throw std::runtime_error("E_ACTIVATION_RECOVERY_EXISTS");
        fs::rename(pending, SnapshotIoPath(destination / "pending.json"));
    } catch (const fs::filesystem_error&) {
        // Retain all evidence on error, including an allocated archive directory.
        throw std::runtime_error("E_ACTIVATION_RECOVERY_WRITE");
    }
    return destination / "pending.json";
}

// Internal commit primitive: caller holds the coordination lock and has verified
// the complete activation receipt. Never call this to bypass candidate checks.
inline void CommitActiveProfilePointer(const std::filesystem::path& data, const std::string& bytes) {
    namespace fs = std::filesystem;
    const auto pending = SnapshotIoPath(data / "active-profile.json.pending");
    const auto pointer = SnapshotIoPath(data / "active-profile.json");
#ifdef _WIN32
    HANDLE file = CreateFileW(pending.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        throw std::runtime_error(error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS ?
            "E_ACTIVATION_PENDING" : "E_ACTIVATION_WRITE");
    }
    DWORD written = 0;
    const bool saved = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size() && FlushFileBuffers(file);
    const bool closed = CloseHandle(file) != 0;
    if (!saved || !closed) throw std::runtime_error("E_ACTIVATION_WRITE");
    if (!MoveFileExW(pending.c_str(), pointer.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("E_ACTIVATION_WRITE");
#else
    const int file = ::open(pending.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) throw std::runtime_error(errno == EEXIST ? "E_ACTIVATION_PENDING" : "E_ACTIVATION_WRITE");
    const bool saved = ::write(file, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size()) && ::fsync(file) == 0;
    const int closed = ::close(file);
    if (!saved || closed != 0) throw std::runtime_error("E_ACTIVATION_WRITE");
    std::error_code error;
    fs::rename(pending, pointer, error);
    if (error) throw std::runtime_error("E_ACTIVATION_WRITE");
#endif
}

// Caller holds data-root, active-profile and candidate-profile exclusive locks.
// Pending evidence is never silently reused. The pointer is the commit record;
// candidate metadata stays staged so there is no second non-atomic state change.
inline void ActivateProfileCandidate(const std::filesystem::path& data, const std::string& id,
    const Json& engine, const SnapshotDigest& digest) {
    namespace fs = std::filesystem;
    if (!ValidSessionId(id)) throw std::runtime_error("E_CANDIDATE_DATA");
    const auto active = ResolveActiveProfile(data, engine);
    const auto candidate = data / "candidates" / id;
    if (active.lexically_normal() == (candidate / "profile").lexically_normal())
        throw std::runtime_error("E_ACTIVATION_ALREADY_ACTIVE");
    for (const auto& directory : {data / "candidates", candidate, candidate / "profile"}) {
        const auto status = fs::symlink_status(SnapshotIoPath(directory));
        if (fs::is_symlink(status) || !fs::is_directory(status)) throw std::runtime_error("E_CANDIDATE_DATA");
    }
    const auto metadata = ReadCandidateDocument(candidate / "candidate.json");
    if (!metadata.contains("sourceSnapshotId") || !metadata["sourceSnapshotId"].is_string() ||
        !ValidSessionId(metadata["sourceSnapshotId"].get<std::string>())) throw std::runtime_error("E_CANDIDATE_DATA");
    const auto receipt = VerifyCandidateActivation(candidate,
        data / "snapshots" / metadata["sourceSnapshotId"].get<std::string>(), active, data / "verified", engine, digest);
    Json state = {{"schema", 1}};
    for (const auto* key : {"candidateId", "verificationId", "sourceSnapshotId", "engine", "adapter"})
        state[key] = receipt[key];
    const auto bytes = state.dump(2) + "\n";
    CommitActiveProfilePointer(data, bytes);
}

// Caller holds data-root, current profile and rollback-candidate exclusive locks.
inline void ActivateProfileRollback(const std::filesystem::path& data, const std::string& id,
    const Json& engine, const SnapshotDigest& digest) {
    if (!ValidSessionId(id)) throw std::runtime_error("E_ROLLBACK_DATA");
    const auto candidate = data / "rollback-candidates" / id;
    VerifyRollbackDirectory(data, candidate / "profile");
    VerifyRollbackDirectory(data, data / "rollback-verified");
    const auto evidence = ReadCandidateDocument(candidate / "rollback-validation.json");
    const auto plan = ReadCandidateDocument(candidate / "rollback.json");
    if (evidence.value("schema", 0) != 1 || !evidence.contains("plan") || evidence["plan"] != plan ||
        plan.value("schema", 0) != 1 || plan.value("state", "") != "prepared" ||
        plan.value("candidateId", "") != id || plan.value("adapter", "") != "stream-json-v1" ||
        !ValidCandidateEngine(engine) || !plan.contains("targetEngine") || plan["targetEngine"] != engine ||
        !plan.contains("priorActivePointer")) throw std::runtime_error("E_ROLLBACK_DATA");
    for (const auto* key : {"sourceSnapshotId", "preservationSnapshotId"})
        if (!plan.contains(key) || !plan[key].is_string() || !ValidSessionId(plan[key].get<std::string>()))
            throw std::runtime_error("E_ROLLBACK_DATA");
    const auto current = ResolveProfileForBackup(data);
    if (current.lexically_normal() == (candidate / "profile").lexically_normal())
        throw std::runtime_error("E_ACTIVATION_ALREADY_ACTIVE");
    const auto preservation = data / "snapshots" / plan.at("preservationSnapshotId").get<std::string>();
    const auto source = data / "snapshots" / plan["sourceSnapshotId"].get<std::string>();
    VerifyRollbackDirectory(data, preservation / "profile");
    VerifyRollbackDirectory(data, source / "profile");
    VerifyProfileSnapshot(source, digest);
    const auto pointer = SnapshotIoPath(data / "active-profile.json");
    const auto prior = std::filesystem::exists(std::filesystem::symlink_status(pointer)) ?
        ReadCandidateDocument(pointer) : Json(nullptr);
    if (prior != plan.at("priorActivePointer") ||
        CandidateFiles(current, digest) != VerifyProfileSnapshot(preservation, digest)["files"])
        throw std::runtime_error("E_ROLLBACK_CHANGED");
    const auto receipt = VerifyCandidateValidation(candidate, data / "rollback-verified", engine, digest);
    if (!evidence.contains("validation") || evidence["validation"] != receipt ||
        receipt["scope"] != "all-top-level-sessions" || receipt["sourceSnapshotId"] != plan["sourceSnapshotId"])
        throw std::runtime_error("E_ROLLBACK_DATA");
    Json state = {{"schema", 2}, {"profileKind", "rollback"},
        {"preservationSnapshotId", evidence.at("plan").at("preservationSnapshotId")}};
    for (const auto* key : {"candidateId", "verificationId", "sourceSnapshotId", "engine", "adapter"})
        state[key] = receipt[key];
    CommitActiveProfilePointer(data, state.dump(2) + "\n");
}

}
