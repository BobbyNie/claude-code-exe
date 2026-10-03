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
    struct Worker { SOCKET socket; std::thread thread; std::atomic<bool> done{false}; };
    std::wstring host, basePath;
    INTERNET_PORT port = 0;
    bool secure = false;
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
    void Forward(SOCKET client, const GatewayRequest& incoming) {
        bool responded = false;
        try {
            const auto request = GatewayForwardRequest(incoming, NarrowHeader(host), port,
                NarrowHeader(basePath), !secure && proxy.active(), proxy.authorization);
            auto exchange = [&](auto& upstream) {
                upstream.Send(request);
                upstream.Send(incoming.body);
                GatewayResponseFraming framing(incoming.method == "HEAD");
                while (!stopping.load()) {
                    auto record = upstream.Read();
                    if (record.ended) {
                        // Plain HTTP has no TLS close_notify. Inner HTTPS still
                        // requires its own authenticated EOF when close-delimited.
                        framing.EndOfStream(!secure || record.authenticated);
                        return;
                    }
                    const bool complete = framing.Feed(record.bytes);
                    responded = true;
                    if (!Send(client, record.bytes) || complete) return;
                }
            };
            if (secure) {
                GatewayTls upstream(host, port, *scopedTrust, stopping, proxy);
                exchange(upstream);
            } else {
                if (proxy.secure) throw std::runtime_error("E_NETWORK");
                GatewaySocket upstream(proxy.active() ? Wide(proxy.host) : host,
                                       proxy.active() ? proxy.port : port, stopping);
                exchange(upstream);
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
