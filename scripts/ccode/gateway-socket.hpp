#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

namespace ccode {
struct GatewayTlsRead { std::string bytes; bool ended = false, authenticated = false; };

// One explicitly selected endpoint. No machine proxy lookup or direct fallback.
class GatewaySocket {
    SOCKET socket = INVALID_SOCKET;
    const std::atomic<bool>& stopping;
    void Ready(bool writing, ULONGLONG deadline) {
        while (!stopping.load() && GetTickCount64() < deadline) {
            WSAPOLLFD wait{socket, static_cast<SHORT>(writing ? POLLWRNORM : POLLRDNORM), 0};
            const int result = WSAPoll(&wait, 1, 100);
            if (result == SOCKET_ERROR) throw std::runtime_error("E_NETWORK");
            if (result > 0) return; // recv/send supplies the actual error or EOF.
        }
        throw std::runtime_error("E_NETWORK");
    }
    void Connect(const std::wstring& connectHost, unsigned short port) {
        ADDRINFOEXW hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        PADDRINFOEXW addresses = nullptr;
        OVERLAPPED operation{}; operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!operation.hEvent) throw std::runtime_error("E_NETWORK");
        HANDLE cancel = nullptr;
        timeval timeout{10, 0};
        const auto service = std::to_wstring(port);
        int result = GetAddrInfoExW(connectHost.c_str(), service.c_str(), NS_ALL, nullptr, &hints,
            &addresses, &timeout, &operation, nullptr, &cancel);
        if (result == WSA_IO_PENDING) {
            const auto deadline = GetTickCount64() + 10000;
            DWORD waited = WAIT_TIMEOUT;
            while (!stopping.load() && GetTickCount64() < deadline &&
                   (waited = WaitForSingleObject(operation.hEvent, 100)) == WAIT_TIMEOUT) {}
            if (waited != WAIT_OBJECT_0) {
                GetAddrInfoExCancel(&cancel);
                // The resolver owns the OVERLAPPED until completion, even on cancel.
                WaitForSingleObject(operation.hEvent, INFINITE);
            }
            result = GetAddrInfoExOverlappedResult(&operation);
        }
        CloseHandle(operation.hEvent);
        std::unique_ptr<ADDRINFOEXW, decltype(&FreeAddrInfoExW)> release(addresses, FreeAddrInfoExW);
        if (stopping.load()) throw std::runtime_error("E_NETWORK");
        if (result) throw std::runtime_error(result == WSAHOST_NOT_FOUND ? "E_GATEWAY_DNS" : "E_NETWORK");
        const auto deadline = GetTickCount64() + 10000;
        for (auto address = addresses; address && !stopping.load(); address = address->ai_next) {
            socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (socket == INVALID_SOCKET) continue;
            u_long nonblocking = 1;
            if (ioctlsocket(socket, FIONBIO, &nonblocking)) { closesocket(socket); socket = INVALID_SOCKET; continue; }
            int connected = connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen));
            if (connected == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {
                Ready(true, deadline);
                int error = 0, length = sizeof(error);
                connected = getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && !error ? 0 : SOCKET_ERROR;
            }
            if (!connected) return;
            closesocket(socket); socket = INVALID_SOCKET;
        }
        throw std::runtime_error("E_NETWORK");
    }
public:
    GatewaySocket(const std::wstring& host, unsigned short port, const std::atomic<bool>& cancelled)
        : stopping(cancelled) {
        try { Connect(host, port); }
        catch (...) { if (socket != INVALID_SOCKET) closesocket(socket); throw; }
    }
    ~GatewaySocket() { if (socket != INVALID_SOCKET) closesocket(socket); }
    GatewaySocket(const GatewaySocket&) = delete;
    GatewaySocket& operator=(const GatewaySocket&) = delete;
    void WireSend(const char* bytes, size_t length) {
        const auto deadline = GetTickCount64() + 600000;
        while (length && !stopping.load()) {
            Ready(true, deadline);
            const int count = send(socket, bytes, static_cast<int>((std::min)(length, size_t(16384))), 0);
            if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
            if (count <= 0) throw std::runtime_error("E_NETWORK");
            bytes += count; length -= count;
        }
        if (length) throw std::runtime_error("E_NETWORK");
    }
    GatewayTlsRead Read() {
        const auto deadline = GetTickCount64() + 600000;
        std::array<char, 16384> bytes{};
        while (!stopping.load()) {
            Ready(false, deadline);
            const int count = recv(socket, bytes.data(), static_cast<int>(bytes.size()), 0);
            if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
            if (count < 0) throw std::runtime_error("E_NETWORK");
            if (!count) return {{}, true, false};
            return {std::string(bytes.data(), count), false, false};
        }
        throw std::runtime_error("E_NETWORK");
    }
    void Send(const std::string& bytes) { WireSend(bytes.data(), bytes.size()); }
};
}
