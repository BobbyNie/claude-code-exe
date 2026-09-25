#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <climits>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include "environment.hpp"
#include "profile.hpp"
#include "sessions.hpp"
#include "permission.hpp"

namespace fs = std::filesystem;
using ccode::Json;
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    void reset(HANDLE h = INVALID_HANDLE_VALUE) {
        if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        value = h;
    }
    operator HANDLE() const { return value; }
    bool valid() const { return value && value != INVALID_HANDLE_VALUE; }
};
std::atomic<HANDLE> activeJob{nullptr};
BOOL WINAPI OnControl(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        auto job = activeJob.load();
        if (job) { TerminateJobObject(job, 130); return TRUE; }
    }
    return FALSE;
}
std::wstring Wide(const std::string& value) {
    if (value.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0);
    if (!n) throw std::runtime_error("E_ENCODING");
    std::wstring result(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), result.data(), n);
    return result;
}
std::string Utf8(const std::wstring& value) {
    if (value.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
    std::string result(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), result.data(), n, nullptr, nullptr);
    return result;
}
std::wstring Env(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (!n) return {};
    std::wstring value(n, 0);
    GetEnvironmentVariableW(name, value.data(), n);
    value.resize(n - 1);
    return value;
}
fs::path Module() {
    std::wstring path(32768, 0);
    DWORD n = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
    if (!n || n == path.size()) throw std::runtime_error("E_LOCATION");
    path.resize(n);
    return path;
}
std::wstring Quote(const std::wstring& value) {
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') ++slashes;
        else if (ch == L'\"') { result.append(slashes * 2 + 1, L'\\'); result += ch; slashes = 0; }
        else { result.append(slashes, L'\\'); result += ch; slashes = 0; }
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}
struct Resource { const unsigned char* bytes; DWORD size; };
Resource Load(int id) {
    auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    auto loaded = resource ? LoadResource(nullptr, resource) : nullptr;
    auto bytes = loaded ? static_cast<const unsigned char*>(LockResource(loaded)) : nullptr;
    auto size = resource ? SizeofResource(nullptr, resource) : 0;
    if (!bytes || !size) throw std::runtime_error("E_RESOURCE");
    return {bytes, size};
}
Json Metadata() {
    auto resource = Load(102);
    return Json::parse(resource.bytes, resource.bytes + resource.size);
}
std::string Digest(Resource resource) {
    unsigned char digest[32];
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
        const_cast<PUCHAR>(resource.bytes), resource.size, digest, sizeof(digest)) < 0)
        throw std::runtime_error("E_CHECKSUM");
    const char* hex = "0123456789abcdef";
    std::string result;
    for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
fs::path PrepareRuntime(const fs::path& directory, const Json& metadata) {
    auto resource = Load(101);
    auto hash = Digest(resource);
    if (hash != metadata.at("sha256").get<std::string>()) throw std::runtime_error("E_CHECKSUM");
    auto runtime = directory / L"runtime" / Wide(hash);
    fs::create_directories(runtime);
    Handle lock(CreateFileW((runtime / L"prepare.lock").c_str(), GENERIC_WRITE, 0, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!lock.valid()) throw std::runtime_error("E_RUNTIME_BUSY");
    auto payload = runtime / L"engine.exe";
    bool equal = fs::exists(payload) && fs::file_size(payload) == resource.size;
    if (equal) {
        std::ifstream file(payload, std::ios::binary);
        char buffer[65536];
        for (size_t offset = 0; offset < resource.size; offset += sizeof(buffer)) {
            auto count = std::min<size_t>(sizeof(buffer), resource.size - offset);
            file.read(buffer, count);
            if (!file || std::memcmp(buffer, resource.bytes + offset, count)) { equal = false; break; }
        }
    }
    if (!equal) {
        auto temporary = runtime / L"engine.new";
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(resource.bytes), resource.size);
        file.close();
        if (!file || !MoveFileExW(temporary.c_str(), payload.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("E_EXTRACT");
    }
    return payload;
}
bool AskPermission(const Json& args) {
    if (Env(L"CCODE_INTERACTIVE") != L"1") return false;
    // The engine may launch MCP workers in a separate hidden console. Never ask
    // for approval there: explicitly use the foreground frontend's console.
    const auto owner = Env(L"CCODE_FRONTEND_PID");
    if (owner.empty() || owner.find_first_not_of(L"0123456789") != std::wstring::npos) return false;
    wchar_t* end = nullptr;
    const auto pid = wcstoul(owner.c_str(), &end, 10);
    if (!pid || *end || pid == ULONG_MAX) return false;
    const HANDLE standard[] = {GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE),
                               GetStdHandle(STD_ERROR_HANDLE)};
    FreeConsole();
    const bool attached = AttachConsole(static_cast<DWORD>(pid)) != FALSE;
    // Attaching a console must not replace the MCP JSON-RPC pipe handles.
    SetStdHandle(STD_INPUT_HANDLE, standard[0]);
    SetStdHandle(STD_OUTPUT_HANDLE, standard[1]);
    SetStdHandle(STD_ERROR_HANDLE, standard[2]);
    if (!attached) return false;
    Handle input(CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, 0, nullptr));
    Handle output(CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr));
    if (!input.valid() || !output.valid()) return false;
    auto text = Wide("\nApprove tool operation?\n" + ccode::ConsoleText(args.dump(2)) + "\nType yes to allow once; Enter denies: ");
    DWORD written = 0, read = 0;
    WriteConsoleW(output, text.data(), (DWORD)text.size(), &written, nullptr);
    wchar_t answer[128]{};
    if (!ReadConsoleW(input, answer, 127, &read, nullptr)) return false;
    std::wstring value(answer, read);
    while (!value.empty() && (value.back() == L'\r' || value.back() == L'\n')) value.pop_back();
    return value == L"yes";
}
int PermissionServer() {
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            if (line.size() > 16 * 1024 * 1024) return 65;
            auto response = ccode::PermissionRpc(Json::parse(line), AskPermission);
            if (!response.is_null()) std::cout << response.dump() << std::endl;
        } catch (...) {
            std::cout << Json{{"jsonrpc", "2.0"}, {"id", nullptr},
                {"error", {{"code", -32600}, {"message", "Invalid request"}}}}.dump() << std::endl;
        }
    }
    return 0;
}
struct Options {
    bool print = false, sessions = false, resumePicker = false, latest = false;
    fs::path data;
    std::string prompt, session;
    std::vector<std::wstring> engine;
};
Options Parse(int argc, wchar_t** argv, const fs::path& module) {
    Options options;
    options.data = Env(L"CCODE_DATA_DIR").empty() ? module.parent_path() / L"data" / L"cc" : fs::path(Env(L"CCODE_DATA_DIR"));
    bool literal = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        auto next = [&]() -> std::wstring {
            if (i + 1 == argc) throw std::runtime_error("E_ARGUMENT");
            return argv[++i];
        };
        if (!literal && arg == L"--") { literal = true; continue; }
        if (!literal && arg == L"--data-dir") options.data = next();
        else if (!literal && (arg == L"--print" || arg == L"-p")) options.print = true;
        else if (!literal && arg == L"--sessions") options.sessions = true;
        else if (!literal && (arg == L"--resume" || arg == L"-r")) {
            if (i + 1 < argc && argv[i + 1][0] != L'-') options.session = Utf8(next());
            else options.resumePicker = true;
        } else if (!literal && (arg == L"--continue" || arg == L"-c")) options.latest = true;
        else if (!literal && (arg == L"--model" || arg == L"--max-turns" || arg == L"--max-budget-usd" ||
            arg == L"--allowedTools" || arg == L"--disallowedTools" || arg == L"--tools" ||
            arg == L"--add-dir" || arg == L"--plugin-dir" || arg == L"--mcp-config" || arg == L"--settings" ||
            arg == L"--agent" || arg == L"--agents" || arg == L"--append-system-prompt" || arg == L"--permission-mode")) {
            auto value = next();
            if (arg == L"--permission-mode" && value != L"default" && value != L"plan" && value != L"acceptEdits")
                throw std::runtime_error("E_PERMISSION_MODE");
            options.engine.push_back(arg); options.engine.push_back(value);
        } else if (!literal && !arg.empty() && arg[0] == L'-') throw std::runtime_error("E_UNSUPPORTED_OPTION");
        else {
            if (!literal && (arg == L"login" || arg == L"logout" || arg == L"setup-token" || arg == L"auth"))
                throw std::runtime_error("E_API_ONLY");
            if (!options.prompt.empty()) options.prompt += " ";
            options.prompt += Utf8(arg);
        }
    }
    if (!options.session.empty() && !ccode::ValidSessionId(options.session)) throw std::runtime_error("E_SESSION_ID");
    options.data = fs::absolute(options.data).lexically_normal();
    return options;
}
std::vector<wchar_t> ChildEnvironment(const fs::path& profile, bool interactive) {
    std::vector<std::wstring> source;
    auto block = GetEnvironmentStringsW();
    if (!block) throw std::runtime_error("E_ENVIRONMENT");
    for (auto cursor = block; *cursor; cursor += wcslen(cursor) + 1) source.emplace_back(cursor);
    FreeEnvironmentStringsW(block);
    source.push_back(interactive ? L"CCODE_INTERACTIVE=1" : L"CCODE_INTERACTIVE=0");
    source.push_back(L"CCODE_FRONTEND_PID=" + std::to_wstring(GetCurrentProcessId()));
    auto entries = ccode::BuildEnvironment(source, profile);
    std::vector<wchar_t> result;
    for (auto& entry : entries) { result.insert(result.end(), entry.begin(), entry.end()); result.push_back(0); }
    result.push_back(0);
    return result;
}
int RunTurn(const fs::path& module, const fs::path& payload, const fs::path& profile,
            const Options& options, bool interactive, const std::string& prompt, std::string& session) {
    auto environment = ChildEnvironment(profile, interactive);
    Json mcp = {{"mcpServers", {{"ccode_permissions", {{"type", "stdio"},
        {"command", module.u8string()}, {"args", {"--ccode-permission-server"}}}}}}};
    std::wstring command = Quote(payload.wstring()) +
        L" --print --output-format stream-json --verbose --mcp-config " + Quote(Wide(mcp.dump())) +
        L" --permission-prompt-tool mcp__ccode_permissions__approve";
    for (const auto& arg : options.engine) command += L" " + Quote(arg);
    if (!session.empty()) command += L" --resume " + Quote(Wide(session));
    std::vector<wchar_t> cmd(command.begin(), command.end()); cmd.push_back(0);
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    Handle outRead, outWrite, inRead, inWrite, err;
    if (!CreatePipe(&outRead.value, &outWrite.value, &security, 0) ||
        !CreatePipe(&inRead.value, &inWrite.value, &security, 0)) throw std::runtime_error("E_PIPE");
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);
    err.reset(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr));
    if (!err.valid()) throw std::runtime_error("E_PIPE");
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> attributes(bytes);
    auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes)) throw std::runtime_error("E_PROCESS");
    HANDLE inherit[] = {inRead.value, outWrite.value, err.value};
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit), nullptr, nullptr)) {
        DeleteProcThreadAttributeList(list); throw std::runtime_error("E_PROCESS");
    }
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = inRead; startup.StartupInfo.hStdOutput = outWrite; startup.StartupInfo.hStdError = err;
    startup.lpAttributeList = list;
    Handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.valid() || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        DeleteProcThreadAttributeList(list); throw std::runtime_error("E_PROCESS");
    }
    PROCESS_INFORMATION child{};
    BOOL created = CreateProcessW(payload.c_str(), cmd.data(), nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT | CREATE_NEW_PROCESS_GROUP,
        environment.data(), nullptr, &startup.StartupInfo, &child);
    DeleteProcThreadAttributeList(list);
    if (!created) throw std::runtime_error("E_START");
    Handle process(child.hProcess), thread(child.hThread);
    if (!AssignProcessToJobObject(job, process)) {
        TerminateProcess(process, 71); throw std::runtime_error("E_PROCESS_TREE");
    }
    outWrite.reset(); inRead.reset(); err.reset();
    activeJob.store(job);
    ResumeThread(thread);
    std::thread writer([&] {
        size_t offset = 0;
        while (offset < prompt.size()) {
            DWORD written = 0;
            if (!WriteFile(inWrite, prompt.data() + offset, (DWORD)std::min<size_t>(65536, prompt.size() - offset), &written, nullptr) || !written) break;
            offset += written;
        }
        inWrite.reset();
    });
    ccode::EventReader reader;
    std::string protocolError;
    char buffer[16384]; DWORD read;
    while (ReadFile(outRead, buffer, sizeof(buffer), &read, nullptr) && read) {
        try { std::cout << reader.Feed(std::string(buffer, read)) << std::flush; }
        catch (const ccode::ProtocolError& error) {
            protocolError = error.what(); TerminateJobObject(job, 65); break;
        }
        catch (...) { protocolError = "E_PROTOCOL"; TerminateJobObject(job, 65); break; }
    }
    WaitForSingleObject(process, INFINITE);
    writer.join();
    activeJob.store(nullptr);
    DWORD code = 1; GetExitCodeProcess(process, &code);
    if (ccode::ValidSessionId(reader.session)) session = reader.session;
    if (code == 130) { std::cerr << "[Cancelled]\n"; return 130; }
    if (!protocolError.empty()) { std::cerr << "[" << protocolError << ": invalid engine event]\n"; return 65; }
    try { reader.Finish(); }
    catch (const ccode::ProtocolError& error) {
        std::cerr << "[" << error.what() << ": incomplete turn]\n";
        return code ? (int)code : 65;
    }
    catch (...) { std::cerr << "[E_ENGINE: incomplete turn; check gateway and configuration]\n"; return code ? (int)code : 65; }
    return code ? (int)code : reader.failed ? 1 : 0;
}
void PrintSessions(const std::vector<ccode::Session>& sessions) {
    if (sessions.empty()) std::cout << "No saved sessions for this workspace.\n";
    for (size_t i = 0; i < sessions.size(); ++i)
        std::cout << i + 1 << ". " << sessions[i].id << "  " << sessions[i].title << '\n';
}
void PickSession(const fs::path& profile, std::string& session) {
    auto sessions = ccode::ListSessions(profile / L"home" / L".claude" / L"projects", fs::current_path());
    PrintSessions(sessions);
    if (sessions.empty()) return;
    std::cout << "Session number (Enter cancels): " << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer) || answer.empty()) return;
    try {
        size_t end = 0, index = std::stoul(answer, &end);
        if (end != answer.size() || !index || index > sessions.size()) throw std::runtime_error("bad index");
        session = sessions[index - 1].id;
        std::cout << "Selected " << session << '\n';
    } catch (...) { std::cout << "Invalid selection.\n"; }
}
int Main(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
    if (argc == 2 && std::wstring(argv[1]) == L"--ccode-permission-server") return PermissionServer();
    auto module = Module();
    if (argc == 2 && (std::wstring(argv[1]) == L"--help" || std::wstring(argv[1]) == L"-h")) {
        std::cout << "ccode - portable coding assistant\n"
            "Usage: ccode [options] [prompt]\n"
            "  --print, -p             Run one turn (also accepts piped input)\n"
            "  --resume [session-id]   Resume a session or open the picker\n"
            "  --continue             Use the latest session in this workspace\n"
            "  --sessions             List saved sessions for this workspace\n"
            "  --data-dir PATH        Persistent data (or CCODE_DATA_DIR)\n"
            "  --model NAME           Select a model\n"
            "  --allowedTools RULE    Explicit tool permission rule\n"
            "  --settings PATH        Engine settings file\n"
            "  --mcp-config PATH      Additional tool servers\n"
            "  --version              Package and engine version\n"
            "Interactive: /resume, /new, /exit; Ctrl+C cancels the running turn.\n"
            "Set A_AUTH_TOKEN or A_API_KEY, and A_BASE_URL. No OS sandbox is provided.\n";
        return 0;
    }
    auto metadata = Metadata();
    if (argc == 2 && std::wstring(argv[1]) == L"--version") {
        std::cout << "ccode 1.0 (engine " << metadata.at("version").get<std::string>() << ")\n"; return 0;
    }
    if (argc == 2 && std::wstring(argv[1]) == L"--ccode-self-test") {
        if (Digest(Load(101)) != metadata.at("sha256").get<std::string>()) throw std::runtime_error("E_CHECKSUM");
        std::cout << "ccode self-test ok\n"; return 0;
    }
    auto options = Parse(argc, argv, module);
    auto profile = options.data / L"profile";
    fs::create_directories(profile);
    Handle lock(CreateFileW((profile / L"frontend.lock").c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!lock.valid()) throw std::runtime_error("E_PROFILE_BUSY");
    for (auto name : {L"home", L"roaming", L"local", L"temp"}) fs::create_directories(profile / name);
    ccode::RestoreLegacyProfile(profile / L"home");
    auto sessions = ccode::ListSessions(profile / L"home" / L".claude" / L"projects", fs::current_path());
    if (options.sessions) { PrintSessions(sessions); return 0; }
    if (options.latest) {
        if (sessions.empty()) throw std::runtime_error("E_NO_SESSION");
        options.session = sessions.front().id;
    }
    if (!ccode::HasApiCredential(Env(L"A_API_KEY"), Env(L"A_AUTH_TOKEN"))) throw std::runtime_error("E_CREDENTIAL: set A_AUTH_TOKEN or A_API_KEY");
    if (!ccode::IsValidGatewayUrl(Env(L"A_BASE_URL"))) throw std::runtime_error("E_GATEWAY: set A_BASE_URL");
    if (options.print && options.resumePicker) throw std::runtime_error("E_SESSION_ID");
    DWORD mode = 0;
    bool console = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0;
    auto payload = PrepareRuntime(module.parent_path(), metadata);
    SetConsoleCtrlHandler(OnControl, TRUE);
    if (options.print) {
        if (!console) {
            std::string piped((std::istreambuf_iterator<char>(std::cin)), {});
            if (!piped.empty()) options.prompt = piped + (options.prompt.empty() ? "" : "\n" + options.prompt);
        }
        if (options.prompt.empty()) throw std::runtime_error("E_PROMPT");
        return RunTurn(module, payload, profile, options, false, options.prompt, options.session);
    }
    std::cout << "ccode - portable coding assistant\n/resume  /new  /exit\n";
    if (options.resumePicker) PickSession(profile, options.session);
    int code = 0;
    if (!options.prompt.empty()) code = RunTurn(module, payload, profile, options, console, options.prompt, options.session);
    std::string prompt;
    while (std::cout << "ccode> " << std::flush, std::getline(std::cin, prompt)) {
        if (prompt == "/exit" || prompt == "/quit") break;
        if (prompt == "/new") { options.session.clear(); continue; }
        if (prompt == "/resume") { PickSession(profile, options.session); continue; }
        if (prompt.empty()) continue;
        code = RunTurn(module, payload, profile, options, console, prompt, options.session);
    }
    return code;
}
}
int wmain(int argc, wchar_t** argv) {
    try { return Main(argc, argv); }
    catch (const std::exception& error) {
        std::string message = error.what();
        // Library exceptions may contain user content or internal absolute paths.
        if (message.rfind("E_", 0) != 0) message = "E_LOCAL: operation failed; check data access and configuration";
        std::cerr << message << '\n';
        return message.rfind("E_PROFILE_BUSY", 0) == 0 ? 75 : 64;
    }
}
