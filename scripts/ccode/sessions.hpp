#pragma once
#include "frontend.hpp"
#include "common.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

namespace ccode {
inline bool ValidSessionId(const std::string& id) {
    if (id.size() != 36) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (id[i] != '-') return false; }
        else if (!std::isxdigit(static_cast<unsigned char>(id[i]))) return false;
    }
    return true;
}
struct Session {
    std::string id, title;
    std::filesystem::file_time_type modified;
};
inline bool SameWorkspace(const std::filesystem::path& a, const std::filesystem::path& b) {
    auto left = std::filesystem::weakly_canonical(a).wstring();
    auto right = std::filesystem::weakly_canonical(b).wstring();
#ifdef _WIN32
    return Lower(left) == Lower(right);
#else
    return left == right;
#endif
}
inline std::vector<Session> ListSessions(const std::filesystem::path& projects,
                                       const std::filesystem::path& workspace) {
    namespace fs = std::filesystem;
    std::vector<Session> result;
    if (!fs::exists(projects)) return result;
    // Read the authoritative transcripts, never a second mutable session database.
    for (const auto& project : fs::directory_iterator(projects)) {
        if (project.is_symlink() || !project.is_directory()) continue;
        for (const auto& file : fs::directory_iterator(project.path())) {
            if (file.is_symlink() || !file.is_regular_file() || file.path().extension() != ".jsonl") continue;
            auto id = file.path().stem().string();
            if (!ValidSessionId(id)) continue;
            std::ifstream input(file.path());
            std::string line;
            while (std::getline(input, line)) {
                if (line.size() > 16 * 1024 * 1024) break;
                auto event = Json::parse(line, nullptr, false);
                if (event.is_discarded() || !event.is_object()) continue;
                // Do not let malformed identity metadata become a generic local
                // error (or an apparently empty history). Never include its value.
                if (event.contains("type") && !event["type"].is_string())
                    throw std::runtime_error("E_SESSION_DATA");
                if (event.value("type", std::string()) != "user") continue;
                if ((event.contains("isSidechain") && !event["isSidechain"].is_boolean()) ||
                    (event.contains("sessionId") && !event["sessionId"].is_string()) ||
                    (event.contains("cwd") && !event["cwd"].is_string()))
                    throw std::runtime_error("E_SESSION_DATA");
                if (event.value("isSidechain", false)) continue;
                if (event.value("sessionId", std::string()) != id || !event.contains("cwd")) continue;
                if (!SameWorkspace(fs::u8path(event.at("cwd").get<std::string>()), workspace)) break;
                std::string title = "Saved session";
                if (event.contains("message") && event["message"].contains("content") &&
                    event["message"]["content"].is_string())
                    title = ConsoleText(event["message"]["content"].get<std::string>());
                std::replace(title.begin(), title.end(), '\n', ' ');
                result.push_back({id, title, file.last_write_time()});
                break;
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const Session& a, const Session& b) {
        return a.modified > b.modified;
    });
    return result;
}
}
