#pragma once
// Include before windows.h. No hook or machine-wide proxy configuration.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <memory>
#include <thread>
#include "gateway-http.hpp"
#include "gateway-response.hpp"
#include "gateway-tls.hpp"
#include <fstream>

namespace ccode {
class GatewayBridge {
    struct InternetHandle {
        HINTERNET value = nullptr;
        explicit InternetHandle(HINTERNET handle = nullptr) : value(handle) {}
        ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
        InternetHandle(const InternetHandle&) = delete;
        InternetHandle& operator=(const InternetHandle&) = delete;
    };
    // Async WinHTTP lets a cancelled turn close an in-flight request without
    // waiting for a model response. The context outlives HANDLE_CLOSING.
    struct Operation {
        HANDLE ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HANDLE closed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HINTERNET request = nullptr;
        std::atomic<DWORD> error{0}, count{0}, secureFailure{0};
        bool callbackInstalled = false;
        explicit Operation(HINTERNET handle) : request(handle) {
            if (!ready || !closed || !request) {
                if (request) WinHttpCloseHandle(request);
                if (ready) CloseHandle(ready);
                if (closed) CloseHandle(closed);
                throw std::runtime_error("E_NETWORK");
            }
        }
        ~Operation() {
            WinHttpCloseHandle(request);
            if (callbackInstalled) WaitForSingleObject(closed, INFINITE);
            CloseHandle(ready); CloseHandle(closed);
        }
        static void CALLBACK Callback(HINTERNET, DWORD_PTR context, DWORD status, void* info, DWORD size) {
            if (!context) return;
            auto& self = *reinterpret_cast<Operation*>(context);
            if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) { SetEvent(self.closed); return; }
            if (status == WINHTTP_CALLBACK_STATUS_SECURE_FAILURE) {
                self.secureFailure.store(*static_cast<DWORD*>(info)); return;
            }
            if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
                self.error.store(static_cast<WINHTTP_ASYNC_RESULT*>(info)->dwError);
            } else if (status == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE) {
                self.count.store(*static_cast<DWORD*>(info));
            } else if (status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) {
                self.count.store(size);
            } else if (status != WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE &&
                       status != WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE) return;
            SetEvent(self.ready);
        }
        void Install() {
            const DWORD_PTR context = reinterpret_cast<DWORD_PTR>(this);
            if (!WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE,
                    const_cast<DWORD_PTR*>(&context), sizeof(context)) ||
                WinHttpSetStatusCallback(request, Callback,
                    WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES | WINHTTP_CALLBACK_FLAG_SECURE_FAILURE, 0) == WINHTTP_INVALID_STATUS_CALLBACK)
                throw std::runtime_error("E_NETWORK");
            callbackInstalled = true;
        }
        bool Await(BOOL started, const std::atomic<bool>& stopping) {
            if (!started) {
                const DWORD code = GetLastError();
                if (code != ERROR_IO_PENDING) { error.store(code); return false; }
            }
            const auto deadline = GetTickCount64() + 600000;
            while (!stopping.load()) {
                const auto result = WaitForSingleObject(ready, 100);
                if (result == WAIT_OBJECT_0) return error.load() == 0;
                if (result != WAIT_TIMEOUT || GetTickCount64() >= deadline) {
                    error.store(ERROR_WINHTTP_TIMEOUT); return false;
                }
            }
            error.store(ERROR_OPERATION_ABORTED); return false;
        }
    };
    struct Worker { SOCKET socket; std::thread thread; std::atomic<bool> done{false}; };
    std::wstring host, basePath;
    INTERNET_PORT port = 0;
    bool secure = false, configuredProxyPolicy = false;
    GatewayProxy proxy;
    std::unique_ptr<GatewayTrust> scopedTrust;
    std::string capability;
    SOCKET listener = INVALID_SOCKET;
    bool winsock = false;
    std::thread acceptThread;
    std::vector<std::unique_ptr<Worker>> workers;
    std::mutex workersMutex;
    std::atomic<bool> stopping{false};
    std::atomic<int> failure{0};
    unsigned short localPort = 0;
    static std::wstring Wide(const std::string& text) {
        if (text.empty()) return {};
        const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (!size) throw std::runtime_error("E_GATEWAY");
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }
    static std::string NarrowHeader(const std::wstring& text) {
        std::string result;
        for (wchar_t ch : text) {
            if (ch < 32 || ch >= 127) throw std::runtime_error("E_NETWORK");
            result += static_cast<char>(ch);
        }
        return result;
    }
    void Failure(DWORD code, DWORD certificateFailure = 0) {
        if (stopping.load()) return;
        int expected = 0;
        // Only the OS result of this actual request supplies these causes.
        const DWORD certificateFlags = WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA |
            WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID | WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID |
            WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED | WINHTTP_CALLBACK_STATUS_FLAG_CERT_WRONG_USAGE |
            WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED | WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT;
        const int value = code == ERROR_WINHTTP_NAME_NOT_RESOLVED ? 1 :
            code == ERROR_WINHTTP_SECURE_FAILURE && (certificateFailure & certificateFlags) ? 3 : 2;
        failure.compare_exchange_strong(expected, value);
    }
    bool Send(SOCKET socket, const char* bytes, size_t length) {
        size_t offset = 0;
        while (offset < length && !stopping.load()) {
            const int sent = send(socket, bytes + offset, static_cast<int>((std::min)(length - offset, size_t(16384))), 0);
            if (sent <= 0) return false;
            offset += sent;
        }
        return offset == length;
    }
    bool Send(SOCKET socket, const std::string& text) { return Send(socket, text.data(), text.size()); }
    void ForwardScopedTls(SOCKET client, const GatewayRequest& incoming) {
        bool responded = false;
        try {
            GatewayTls upstream(host, port, *scopedTrust, stopping, proxy);
            const auto target = NarrowHeader(basePath) + incoming.target;
            if (target.find_first_of(" \t\r\n\\") != std::string::npos) throw std::runtime_error("E_NETWORK");
            auto authority = NarrowHeader(host);
            if (authority.find(':') != std::string::npos) authority = "[" + authority + "]";
            if (port != 443) authority += ":" + std::to_string(port);
            std::string request = incoming.method + " " + target + " HTTP/1.1\r\nHost: " + authority +
                "\r\nConnection: close\r\nAccept-Encoding: identity\r\nContent-Length: " +
                std::to_string(incoming.body.size()) + "\r\n";
            for (const auto& header : incoming.headers) request += header.first + ": " + header.second + "\r\n";
            upstream.Send(request + "\r\n");
            upstream.Send(incoming.body);
            GatewayResponseFraming framing(incoming.method == "HEAD");
            while (!stopping.load()) {
                auto record = upstream.Read();
                if (record.ended) { framing.EndOfStream(record.authenticated); return; }
                const bool complete = framing.Feed(record.bytes);
                responded = true;
                if (!Send(client, record.bytes) || complete) return;
            }
        } catch (const std::runtime_error& error) {
            if (!stopping.load()) {
                const std::string code = error.what();
                int expected = 0;
                failure.compare_exchange_strong(expected, code == "E_GATEWAY_TLS" ? 3 : code == "E_GATEWAY_DNS" ? 1 : 2);
                if (!responded) Send(client, "HTTP/1.1 502 Gateway Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            }
        }
    }
    void Forward(SOCKET client, const GatewayRequest& incoming) {
        if (scopedTrust) { ForwardScopedTls(client, incoming); return; }
        if (proxy.secure) throw std::runtime_error("E_NETWORK");
        const auto proxyAddress = Wide((proxy.host.find(':') == std::string::npos ? proxy.host : "[" + proxy.host + "]") +
                                       ":" + std::to_string(proxy.port));
        InternetHandle session(WinHttpOpen(L"ccode", proxy.active() ? WINHTTP_ACCESS_TYPE_NAMED_PROXY :
            configuredProxyPolicy ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            proxy.active() ? proxyAddress.c_str() : WINHTTP_NO_PROXY_NAME,
            // Selection already applied the caller's NO_PROXY policy. Do not
            // add WinHTTP's implicit loopback bypass to an explicit proxy route.
            proxy.active() ? L"<-loopback>" : WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC));
        if (!session.value || !WinHttpSetTimeouts(session.value, 10000, 10000, 10000, 600000))
            throw std::runtime_error("E_NETWORK");
        InternetHandle connection(WinHttpConnect(session.value, host.c_str(), port, 0));
        if (!connection.value) throw std::runtime_error("E_NETWORK");
        const auto target = basePath + Wide(incoming.target);
        Operation operation(WinHttpOpenRequest(connection.value, Wide(incoming.method).c_str(), target.c_str(),
            L"HTTP/1.1", WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_ESCAPE_DISABLE | (secure ? WINHTTP_FLAG_SECURE : 0)));
        operation.Install();
        DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_REDIRECTS;
        DWORD logon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        if (!WinHttpSetOption(operation.request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)) ||
            !WinHttpSetOption(operation.request, WINHTTP_OPTION_AUTOLOGON_POLICY, &logon, sizeof(logon)))
            throw std::runtime_error("E_NETWORK");
        std::wstring headers = L"Accept-Encoding: identity\r\nConnection: close\r\n";
        if (proxy.active() && !proxy.authorization.empty())
            headers += L"Proxy-Authorization: " + Wide(proxy.authorization) + L"\r\n";
        for (const auto& header : incoming.headers)
            headers += Wide(header.first) + L": " + Wide(header.second) + L"\r\n";
        auto failed = [&] {
            Failure(operation.error.load(), operation.secureFailure.load());
            Send(client, "HTTP/1.1 502 Gateway Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        };
        if (!operation.Await(WinHttpSendRequest(operation.request, headers.c_str(), static_cast<DWORD>(headers.size()),
                incoming.body.empty() ? nullptr : const_cast<char*>(incoming.body.data()),
                static_cast<DWORD>(incoming.body.size()), static_cast<DWORD>(incoming.body.size()),
                reinterpret_cast<DWORD_PTR>(&operation)), stopping)) { failed(); return; }
        if (!operation.Await(WinHttpReceiveResponse(operation.request, nullptr), stopping)) { failed(); return; }
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(operation.request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) || status < 100 || status > 599)
            throw std::runtime_error("E_NETWORK");
        std::string response = "HTTP/1.1 " + std::to_string(status) + " Gateway\r\nConnection: close\r\n";
        for (const auto* name : {L"Content-Type", L"Content-Encoding", L"Retry-After", L"Request-Id"}) {
            std::array<wchar_t, 8192> value{}; DWORD bytes = static_cast<DWORD>(value.size() * sizeof(wchar_t));
            if (WinHttpQueryHeaders(operation.request, WINHTTP_QUERY_CUSTOM, name, value.data(), &bytes, WINHTTP_NO_HEADER_INDEX))
                response += NarrowHeader(name) + ": " + NarrowHeader(value.data()) + "\r\n";
            else if (GetLastError() != ERROR_WINHTTP_HEADER_NOT_FOUND) throw std::runtime_error("E_NETWORK");
        }
        response += "\r\n";
        if (!Send(client, response)) return;
        std::array<char, 16384> buffer{};
        while (!stopping.load()) {
            // ReadData can wait to fill its requested length. Query the available
            // bytes first so an SSE record is forwarded before the next arrives.
            if (!operation.Await(WinHttpQueryDataAvailable(operation.request, nullptr), stopping)) {
                Failure(operation.error.load()); return;
            }
            const DWORD available = operation.count.load();
            if (!available) return;
            const DWORD wanted = (std::min)(available, static_cast<DWORD>(buffer.size()));
            if (!operation.Await(WinHttpReadData(operation.request, buffer.data(), wanted, nullptr), stopping)) {
                Failure(operation.error.load()); return;
            }
            const DWORD read = operation.count.load();
            if (!read) return;
            if (!Send(client, buffer.data(), read)) return;
        }
    }
    void FinishRejectedResponse(SOCKET client) {
        // A body may still be arriving after its headers were rejected. Closing
        // with unread bytes can turn the already-sent 400 into a TCP reset.
        // Half-close first, then drain only a bounded amount/time; never forward.
        if (shutdown(client, SD_SEND) == SOCKET_ERROR) {
            if (!stopping.load()) Failure(0);
            return;
        }
        const auto deadline = GetTickCount64() + 500;
        size_t drained = 0;
        std::array<char, 4096> discard{};
        while (!stopping.load() && drained < 65536 && GetTickCount64() < deadline) {
            WSAPOLLFD poll{client, POLLRDNORM, 0};
            const int ready = WSAPoll(&poll, 1, 25);
            if (ready == SOCKET_ERROR) { Failure(0); return; }
            if (!ready) continue;
            const int count = recv(client, discard.data(), static_cast<int>(discard.size()), 0);
            if (count == 0) return;
            if (count == SOCKET_ERROR) {
                const int error = WSAGetLastError();
                if (error != WSAECONNRESET && error != WSAECONNABORTED && !stopping.load()) Failure(0);
                return;
            }
            drained += static_cast<size_t>(count);
        }
    }
    void Serve(Worker& worker) {
        try {
            GatewayRequestParser parser(capability);
            std::array<char, 16384> buffer{};
            while (!stopping.load()) {
                const int read = recv(worker.socket, buffer.data(), static_cast<int>(buffer.size()), 0);
                if (read <= 0) break;
                if (parser.Feed(std::string(buffer.data(), read))) {
                    if (!failure.load()) Forward(worker.socket, parser.Request());
                    else Send(worker.socket, "HTTP/1.1 503 Gateway Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    break;
                }
            }
        } catch (const std::runtime_error& error) {
            if (std::string(error.what()) == "E_NETWORK") Failure(0);
            Send(worker.socket, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            FinishRejectedResponse(worker.socket);
        } catch (...) { Failure(0); }
        std::lock_guard<std::mutex> lock(workersMutex);
        shutdown(worker.socket, SD_BOTH); closesocket(worker.socket);
        worker.socket = INVALID_SOCKET;
        worker.done.store(true);
    }
    void Accept() {
        while (!stopping.load()) {
            const auto socket = accept(listener, nullptr, nullptr);
            if (socket == INVALID_SOCKET) { if (!stopping.load()) Failure(0); break; }
            try {
                std::lock_guard<std::mutex> lock(workersMutex);
                for (auto it = workers.begin(); it != workers.end();) {
                    if ((*it)->done.load()) { (*it)->thread.join(); it = workers.erase(it); }
                    else ++it;
                }
                if (stopping.load() || workers.size() >= 16) { closesocket(socket); continue; }
                DWORD timeout = 10000;
                if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) ||
                    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout))) {
                    closesocket(socket); Failure(0); continue;
                }
                auto worker = std::make_unique<Worker>(); worker->socket = socket;
                workers.push_back(std::move(worker));
                auto* current = workers.back().get();
                try { current->thread = std::thread([this, current] { Serve(*current); }); }
                catch (...) { workers.pop_back(); throw; }
            } catch (...) { closesocket(socket); Failure(0); }
        }
    }
public:
    // HTTPS always validates the actual Schannel peer before sending the final
    // handshake token; optional extra roots remain invocation-scoped.
    explicit GatewayBridge(const std::wstring& upstream, const std::wstring& extraCaPath = {}) {
        URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
            parts.dwUserNameLength = parts.dwPasswordLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(upstream.c_str(), static_cast<DWORD>(upstream.size()), 0, &parts) ||
            (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) || !parts.dwHostNameLength ||
            parts.dwUserNameLength || parts.dwPasswordLength || parts.dwExtraInfoLength)
            throw std::runtime_error("E_GATEWAY");
        secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
        if (secure) {
            std::string pem;
            if (!extraCaPath.empty()) {
                std::ifstream file(std::filesystem::path(extraCaPath), std::ios::binary | std::ios::ate);
                if (!file || file.tellg() <= 0 || file.tellg() > 4 * 1024 * 1024) throw std::runtime_error("E_GATEWAY_TLS");
                pem.resize(static_cast<size_t>(file.tellg()), '\0');
                file.seekg(0);
                if (!file.read(&pem[0], static_cast<std::streamsize>(pem.size()))) throw std::runtime_error("E_GATEWAY_TLS");
            }
            scopedTrust = std::make_unique<GatewayTrust>(pem);
        }
        host.assign(parts.lpszHostName, parts.dwHostNameLength); port = parts.nPort;
        const auto environment = [](const wchar_t* name) {
            const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
            if (!size) return std::string();
            if (size > 32768) throw std::runtime_error("E_NETWORK");
            std::vector<wchar_t> value(size);
            SetLastError(ERROR_SUCCESS);
            const DWORD copied = GetEnvironmentVariableW(name, value.data(), size);
            ValidateGatewayEnvironmentRead(size, copied, GetLastError());
            return NarrowHeader(std::wstring(value.data(), copied));
        };
        // Windows environment names are case-insensitive. Values stay in memory.
        const auto httpsProxy = environment(L"HTTPS_PROXY"), httpProxy = environment(L"HTTP_PROXY");
        const auto exclusions = environment(L"NO_PROXY");
        configuredProxyPolicy = !httpsProxy.empty() || !httpProxy.empty() || !exclusions.empty();
        proxy = SelectGatewayProxy(NarrowHeader(host), port, httpsProxy, httpProxy, exclusions);
        if (parts.dwUrlPathLength) basePath.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
        while (!basePath.empty() && basePath.back() == L'/') basePath.pop_back();
        std::array<unsigned char, 32> random{};
        if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
            throw std::runtime_error("E_NETWORK");
        capability = "/";
        for (auto byte : random) { capability += "0123456789abcdef"[byte >> 4]; capability += "0123456789abcdef"[byte & 15]; }
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("E_NETWORK");
        winsock = true;
        try {
            listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener == INVALID_SOCKET) throw std::runtime_error("E_NETWORK");
            BOOL exclusive = TRUE;
            if (setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)))
                throw std::runtime_error("E_NETWORK");
            sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(listener, 16))
                throw std::runtime_error("E_NETWORK");
            int size = sizeof(address);
            if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size)) throw std::runtime_error("E_NETWORK");
            localPort = ntohs(address.sin_port);
            acceptThread = std::thread([this] { Accept(); });
        } catch (...) { Stop(); throw; }
    }
    ~GatewayBridge() { Stop(); }
    GatewayBridge(const GatewayBridge&) = delete;
    GatewayBridge& operator=(const GatewayBridge&) = delete;
    std::wstring Url() const { return L"http://127.0.0.1:" + std::to_wstring(localPort) + Wide(capability); }
    std::string Error() const { const int code = failure.load(); return code == 1 ? "E_GATEWAY_DNS" : code == 2 ? "E_NETWORK" : code == 3 ? "E_GATEWAY_TLS" : ""; }
    void Stop() noexcept {
        if (stopping.exchange(true)) return;
        if (listener != INVALID_SOCKET) { closesocket(listener); }
        if (acceptThread.joinable()) acceptThread.join();
        {
            std::lock_guard<std::mutex> lock(workersMutex);
            for (auto& worker : workers)
                if (worker->socket != INVALID_SOCKET) shutdown(worker->socket, SD_BOTH);
        }
        for (auto& worker : workers) if (worker->thread.joinable()) worker->thread.join();
        workers.clear();
        if (winsock) { WSACleanup(); winsock = false; }
    }
};
}
