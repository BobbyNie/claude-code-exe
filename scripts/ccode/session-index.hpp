#pragma once
#include "workspaces.hpp"

namespace ccode {
// Caller holds the profile lock. This cache is never read to decide which
// sessions exist or can resume: only native transcripts have that authority.
inline std::vector<Session> ListWorkspaceSessions(const std::filesystem::path& profile,
                                                 const std::filesystem::path& workspace) {
    namespace fs = std::filesystem;
    const auto identity = ResolveWorkspace(profile / "workspaces.json", workspace);
    auto sessions = ListSessions(profile / "home" / ".claude" / "projects", workspace);
    Json entries = Json::array();
    for (const auto& session : sessions) {
        entries.push_back({{"id", session.id}, {"summary", session.title},
            {"modified", session.modified.time_since_epoch().count()},
            {"engineVersion", session.engineVersion}, {"availability", "discovered"}});
    }
    // 'discovered' does not promise compatibility or a successfully resumed turn.
    const Json document = {{"schema", 1}, {"workspaceId", identity}, {"sessions", entries}};
    const auto directory = NativeIoPath(profile / "session-index");
    if (fs::is_symlink(fs::symlink_status(directory))) throw std::runtime_error("E_SESSION_INDEX_WRITE");
    fs::create_directories(directory);
    const auto target = directory / (identity + ".json");
    auto candidate = target; candidate += ".new";
    if (fs::is_symlink(fs::symlink_status(candidate)) || fs::is_symlink(fs::symlink_status(target)))
        throw std::runtime_error("E_SESSION_INDEX_WRITE");
    {
        std::ofstream output(candidate, std::ios::binary | std::ios::trunc);
        output << document.dump(2) << '\n';
        output.close();
        if (!output) throw std::runtime_error("E_SESSION_INDEX_WRITE");
    }
#ifdef _WIN32
    if (!MoveFileExW(candidate.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("E_SESSION_INDEX_WRITE");
#else
    std::error_code error;
    fs::rename(candidate, target, error);
    if (error) throw std::runtime_error("E_SESSION_INDEX_WRITE");
#endif
    return sessions;
}
}
