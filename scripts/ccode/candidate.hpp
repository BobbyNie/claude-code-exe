#pragma once
#include "snapshot.hpp"
#include "workspaces.hpp"

namespace ccode {
// This is local integrity evidence, not an authenticated security boundary.
// Callers hold both the active-profile and candidate-profile exclusive locks.
using CandidateProbe = std::function<bool(const std::filesystem::path&, const std::string&, const std::string&)>;
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
inline bool ValidCandidateEngine(const Json& engine) {
    if (!engine.is_object() || !engine.contains("version") || !engine["version"].is_string() ||
        engine["version"].get<std::string>().empty() || !engine.contains("sha256") || !engine["sha256"].is_string()) return false;
    const auto hash = engine["sha256"].get<std::string>();
    return hash.size() == 64 && hash.find_first_not_of("0123456789abcdef") == std::string::npos;
}
inline Json ValidateProfileCandidate(const std::filesystem::path& candidatePath,
    const std::filesystem::path& sourceSnapshot, const std::filesystem::path& verifiedRoot,
    const std::string& verificationId, const std::filesystem::path& workspace,
    const std::string& session, const Json& engine, const SnapshotDigest& digest, const CandidateProbe& probe) {
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
    // A live candidate has frontend.lock while an immutable snapshot must not.
    // Validate the full expected manifest and all non-lock files without relaxing
    // VerifyProfileSnapshot's strict rejection of extra backup files.
    auto expectedManifest = source;
    expectedManifest["snapshotId"] = metadata["candidateId"];
    if (ReadCandidateDocument(candidate / "manifest.json") != expectedManifest ||
        CandidateFiles(candidate / "profile", digest) != source["files"])
        throw std::runtime_error("E_CANDIDATE_CHANGED");
    const auto expected = CandidateHistoryText(candidate / "profile", workspace, session);
    if (!probe(candidatePath / "profile", session, expected)) throw std::runtime_error("E_CANDIDATE_HISTORY");
    if (VerifyProfileSnapshot(sourceSnapshot, digest) != source ||
        ReadCandidateDocument(candidate / "candidate.json") != metadata)
        throw std::runtime_error("E_CANDIDATE_CHANGED");
    const auto workspaceId = ResolveWorkspace(SnapshotIoPath(candidate / "profile/workspaces.json"), workspace);
    const auto checkpoint = CreateProfileSnapshot(candidate / "profile", verifiedRoot, verificationId, digest);
    const auto frozen = VerifyProfileSnapshot(checkpoint, digest);
    if (CandidateFiles(candidate / "profile", digest) != frozen["files"]) throw std::runtime_error("E_CANDIDATE_CHANGED");
    const Json receipt = {{"schema", 1}, {"candidateId", metadata["candidateId"]},
        {"sourceSnapshotId", source["snapshotId"]}, {"verificationId", verificationId},
        {"scope", "single-session"}, {"sessionId", session}, {"workspaceId", workspaceId},
        {"engine", engine}, {"adapter", "stream-json-v1"}, {"historyVerified", true},
        {"files", frozen["files"]}};
    const auto pending = candidate / "validation.json.pending";
    std::ofstream output(pending, std::ios::binary);
    output << receipt.dump(2) << '\n';
    output.close();
    if (!output) throw std::runtime_error("E_CANDIDATE_WRITE");
    fs::rename(pending, candidate / "validation.json");
    return receipt;
}
inline Json VerifyCandidateValidation(const std::filesystem::path& candidate,
    const std::filesystem::path& verifiedRoot, const Json& engine, const SnapshotDigest& digest) {
    const auto receipt = ReadCandidateDocument(candidate / "validation.json");
    if (!receipt.contains("schema") || receipt["schema"] != 1 ||
        !receipt.contains("candidateId") || receipt["candidateId"] != candidate.filename().u8string() ||
        !receipt.contains("verificationId") || !receipt["verificationId"].is_string() ||
        !ValidSessionId(receipt["verificationId"].get<std::string>()) ||
        !receipt.contains("sessionId") || !receipt["sessionId"].is_string() || !ValidSessionId(receipt["sessionId"].get<std::string>()) ||
        !receipt.contains("workspaceId") || !receipt["workspaceId"].is_string() || !ValidSessionId(receipt["workspaceId"].get<std::string>()) ||
        !receipt.contains("scope") || receipt["scope"] != "single-session" ||
        !receipt.contains("historyVerified") || receipt["historyVerified"] != true ||
        !receipt.contains("adapter") || receipt["adapter"] != "stream-json-v1" ||
        !receipt.contains("engine") || !ValidCandidateEngine(engine) || receipt["engine"] != engine ||
        !receipt.contains("files")) throw std::runtime_error("E_CANDIDATE_DATA");
    const auto metadata = ReadCandidateDocument(candidate / "candidate.json");
    if (!receipt.contains("sourceSnapshotId") || !receipt["sourceSnapshotId"].is_string() ||
        !ValidSessionId(receipt["sourceSnapshotId"].get<std::string>()) ||
        !metadata.contains("sourceSnapshotId") || receipt["sourceSnapshotId"] != metadata["sourceSnapshotId"] ||
        !metadata.contains("candidateId") || receipt["candidateId"] != metadata["candidateId"])
        throw std::runtime_error("E_CANDIDATE_DATA");
    const auto frozen = VerifyProfileSnapshot(verifiedRoot / receipt["verificationId"].get<std::string>(), digest);
    if (receipt["files"] != frozen["files"] || CandidateFiles(candidate / "profile", digest) != frozen["files"])
        throw std::runtime_error("E_CANDIDATE_CHANGED");
    return receipt;
}
}
