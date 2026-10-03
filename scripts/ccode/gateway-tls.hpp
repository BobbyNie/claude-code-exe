#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <security.h>
#include <schannel.h>
#include <algorithm>
#include <atomic>
#include <array>
#include "gateway-trust.hpp"

namespace ccode {
struct GatewayTlsRead { std::string bytes; bool ended = false, authenticated = false; };

// One actual upstream TLS connection, no preflight/replay and no root-store writes.
class GatewayTls {
    SOCKET socket = INVALID_SOCKET;
    CredHandle credentials{};
    CtxtHandle context{};
    bool haveCredentials = false, haveContext = false;
    SecPkgContext_StreamSizes sizes{};
    std::string encrypted;
    const GatewayTrust& trust;
    const std::atomic<bool>& stopping;
    std::wstring hostname;
    static constexpr size_t RecordLimit = 1024 * 1024;
    struct Token {
        SecBuffer buffer{0, SECBUFFER_TOKEN, nullptr};
        ~Token() { if (buffer.pvBuffer) FreeContextBuffer(buffer.pvBuffer); }
    };
    static void SecurityFailure(SECURITY_STATUS status) {
        if (status == SEC_E_UNTRUSTED_ROOT || status == SEC_E_WRONG_PRINCIPAL || status == SEC_E_CERT_EXPIRED)
            throw std::runtime_error("E_GATEWAY_TLS");
        throw std::runtime_error("E_NETWORK");
    }
    void Ready(bool writing, ULONGLONG deadline) {
        while (!stopping.load() && GetTickCount64() < deadline) {
            WSAPOLLFD wait{socket, static_cast<SHORT>(writing ? POLLWRNORM : POLLRDNORM), 0};
            const int result = WSAPoll(&wait, 1, 100);
            if (result == SOCKET_ERROR) throw std::runtime_error("E_NETWORK");
            if (result > 0) return; // recv/send supplies the actual error or EOF.
        }
        throw std::runtime_error("E_NETWORK");
    }
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
    bool WireRead() {
        const auto deadline = GetTickCount64() + 600000;
        std::array<char, 16384> bytes{};
        while (!stopping.load()) {
            Ready(false, deadline);
            const int count = recv(socket, bytes.data(), static_cast<int>(bytes.size()), 0);
            if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
            if (count < 0) throw std::runtime_error("E_NETWORK");
            if (!count) return false;
            if (encrypted.size() + count > RecordLimit) throw std::runtime_error("E_NETWORK");
            encrypted.append(bytes.data(), count); return true;
        }
        throw std::runtime_error("E_NETWORK");
    }
    void Connect(unsigned short port) {
        ADDRINFOEXW hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        PADDRINFOEXW addresses = nullptr;
        OVERLAPPED operation{}; operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!operation.hEvent) throw std::runtime_error("E_NETWORK");
        HANDLE cancel = nullptr;
        timeval timeout{10, 0};
        const auto service = std::to_wstring(port);
        int result = GetAddrInfoExW(hostname.c_str(), service.c_str(), NS_ALL, nullptr, &hints,
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
    void Handshake(bool initial) {
        const DWORD flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
            ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION;
        bool verified = false, needMore = !initial && encrypted.empty();
        while (!stopping.load()) {
            if (needMore && !WireRead()) throw std::runtime_error("E_NETWORK");
            std::array<SecBuffer, 2> input{{{static_cast<ULONG>(encrypted.size()), SECBUFFER_TOKEN,
                encrypted.empty() ? nullptr : &encrypted[0]}, {0, SECBUFFER_EMPTY, nullptr}}};
            SecBufferDesc incoming{SECBUFFER_VERSION, static_cast<ULONG>(input.size()), input.data()};
            Token output;
            SecBufferDesc outgoing{SECBUFFER_VERSION, 1, &output.buffer};
            DWORD attributes = 0; TimeStamp expiry{};
            const auto status = InitializeSecurityContextW(&credentials, initial ? nullptr : &context,
                const_cast<wchar_t*>(hostname.c_str()), flags, 0, SECURITY_NATIVE_DREP,
                initial ? nullptr : &incoming, 0, &context, &outgoing, &attributes, &expiry);
            if (SecIsValidHandle(&context)) haveContext = true;
            if (status == SEC_E_INCOMPLETE_MESSAGE) { needMore = true; continue; }
            if (status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED) SecurityFailure(status);
            haveContext = true;
            // Schannel manual validation is not permission to accept an invalid
            // peer: validate as soon as the certificate exists, before sending
            // the next handshake token (in particular, the client's Finished).
            PCCERT_CONTEXT peer = nullptr;
            if (!verified && QueryContextAttributesW(&context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &peer) == SEC_E_OK) {
                const auto policy = trust.Verify(peer, hostname);
                CertFreeCertificateContext(peer);
                if (policy != GatewayCertificateResult::Trusted) throw std::runtime_error("E_GATEWAY_TLS");
                verified = true;
            }
            if (status == SEC_E_OK && !(attributes & ISC_RET_CONFIDENTIALITY)) throw std::runtime_error("E_NETWORK");
            if (status == SEC_E_OK && !verified) throw std::runtime_error("E_GATEWAY_TLS");
            if (output.buffer.cbBuffer)
                WireSend(static_cast<const char*>(output.buffer.pvBuffer), output.buffer.cbBuffer);
            std::string extra;
            if (!initial && input[1].BufferType == SECBUFFER_EXTRA && input[1].cbBuffer)
                extra.assign(encrypted.end() - input[1].cbBuffer, encrypted.end());
            encrypted = std::move(extra); initial = false;
            if (status == SEC_E_OK) {
                if (QueryContextAttributesW(&context, SECPKG_ATTR_STREAM_SIZES, &sizes) != SEC_E_OK || !sizes.cbMaximumMessage)
                    throw std::runtime_error("E_NETWORK");
                return;
            }
            needMore = encrypted.empty();
        }
        throw std::runtime_error("E_NETWORK");
    }
    void Cleanup() noexcept {
        if (socket != INVALID_SOCKET) { closesocket(socket); socket = INVALID_SOCKET; }
        if (haveContext) { DeleteSecurityContext(&context); haveContext = false; }
        if (haveCredentials) { FreeCredentialsHandle(&credentials); haveCredentials = false; }
    }
public:
    GatewayTls(const std::wstring& host, unsigned short port, const GatewayTrust& policy, const std::atomic<bool>& cancelled)
        : trust(policy), stopping(cancelled), hostname(host) {
        SecInvalidateHandle(&context);
        SecInvalidateHandle(&credentials);
        try {
            Connect(port);
            SCHANNEL_CRED config{}; config.dwVersion = SCHANNEL_CRED_VERSION;
            config.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO;
            TimeStamp expiry{};
            const auto status = AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W),
                SECPKG_CRED_OUTBOUND, nullptr, &config, nullptr, nullptr, &credentials, &expiry);
            if (status != SEC_E_OK) SecurityFailure(status);
            haveCredentials = true; Handshake(true);
        } catch (...) { Cleanup(); throw; }
    }
    ~GatewayTls() { Cleanup(); }
    GatewayTls(const GatewayTls&) = delete;
    GatewayTls& operator=(const GatewayTls&) = delete;
    void Send(const std::string& plaintext) {
        size_t offset = 0;
        while (offset < plaintext.size()) {
            const ULONG count = static_cast<ULONG>((std::min)(plaintext.size() - offset, size_t(sizes.cbMaximumMessage)));
            std::vector<char> record(sizes.cbHeader + count + sizes.cbTrailer);
            std::copy_n(plaintext.data() + offset, count, record.data() + sizes.cbHeader);
            std::array<SecBuffer, 4> buffers{{{sizes.cbHeader, SECBUFFER_STREAM_HEADER, record.data()},
                {count, SECBUFFER_DATA, record.data() + sizes.cbHeader},
                {sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, record.data() + sizes.cbHeader + count},
                {0, SECBUFFER_EMPTY, nullptr}}};
            SecBufferDesc message{SECBUFFER_VERSION, static_cast<ULONG>(buffers.size()), buffers.data()};
            if (EncryptMessage(&context, 0, &message, 0) != SEC_E_OK) throw std::runtime_error("E_NETWORK");
            for (size_t index = 0; index < 3; ++index)
                WireSend(static_cast<const char*>(buffers[index].pvBuffer), buffers[index].cbBuffer);
            offset += count;
        }
    }
    GatewayTlsRead Read() {
        while (!stopping.load()) {
            if (encrypted.empty() && !WireRead()) return {{}, true, false};
            std::array<SecBuffer, 4> buffers{{{static_cast<ULONG>(encrypted.size()), SECBUFFER_DATA, &encrypted[0]},
                {0, SECBUFFER_EMPTY, nullptr}, {0, SECBUFFER_EMPTY, nullptr}, {0, SECBUFFER_EMPTY, nullptr}}};
            SecBufferDesc message{SECBUFFER_VERSION, static_cast<ULONG>(buffers.size()), buffers.data()};
            const auto status = DecryptMessage(&context, &message, 0, nullptr);
            if (status == SEC_E_INCOMPLETE_MESSAGE) {
                if (!WireRead()) throw std::runtime_error("E_NETWORK");
                continue;
            }
            if (status == SEC_I_CONTEXT_EXPIRED) return {{}, true, true};
            if (status != SEC_E_OK && status != SEC_I_RENEGOTIATE) throw std::runtime_error("E_NETWORK");
            std::string plaintext, extra;
            for (const auto& buffer : buffers) {
                if (buffer.BufferType == SECBUFFER_DATA && buffer.cbBuffer)
                    plaintext.append(static_cast<const char*>(buffer.pvBuffer), buffer.cbBuffer);
                if (buffer.BufferType == SECBUFFER_EXTRA && buffer.cbBuffer)
                    extra.assign(static_cast<const char*>(buffer.pvBuffer), buffer.cbBuffer);
            }
            encrypted = std::move(extra);
            if (status == SEC_I_RENEGOTIATE) { Handshake(false); continue; }
            if (!plaintext.empty()) return {std::move(plaintext), false, false};
        }
        throw std::runtime_error("E_NETWORK");
    }
};
}
