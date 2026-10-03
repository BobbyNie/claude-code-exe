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
#include "gateway-proxy.hpp"
#include "gateway-socket.hpp"

namespace ccode {

// One actual upstream TLS connection, no preflight/replay and no root-store writes.
class GatewayTls {
    std::unique_ptr<GatewaySocket> wire;
    std::unique_ptr<GatewayTls> proxyTls;
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
    void WireSend(const char* bytes, size_t length) {
        if (proxyTls) proxyTls->Send(std::string(bytes, length));
        else wire->WireSend(bytes, length);
    }
    bool WireRead() {
        auto record = proxyTls ? proxyTls->Read() : wire->Read();
        if (record.ended) return false;
        if (record.bytes.size() > RecordLimit - encrypted.size()) throw std::runtime_error("E_NETWORK");
        encrypted += record.bytes;
        return true;
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
            // Unlike REMOTE_CERT_CONTEXT (available too late on TLS 1.2),
            // SDK attribute 0x75 is explicitly valid DURING the SSPI loop.
            // Do not use its 0x74 INPROC counterpart: that is post-handshake.
            struct PeerBlob {
                CERT_BLOB value{};
                ~PeerBlob() { if (value.pbData) FreeContextBuffer(value.pbData); }
            } peer;
            if (!verified && QueryContextAttributesW(&context, 0x75, &peer.value) == SEC_E_OK) {
                const auto certificate = GatewayTrust::DeserializePeer(peer.value);
                if (trust.Verify(certificate.get(), hostname) != GatewayCertificateResult::Trusted)
                    throw std::runtime_error("E_GATEWAY_TLS");
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
        proxyTls.reset();
        wire.reset();
        if (haveContext) { DeleteSecurityContext(&context); haveContext = false; }
        if (haveCredentials) { FreeCredentialsHandle(&credentials); haveCredentials = false; }
    }
public:
    GatewayTls(const std::wstring& host, unsigned short port, const GatewayTrust& policy, const std::atomic<bool>& cancelled,
               const GatewayProxy& proxy = {})
        : trust(policy), stopping(cancelled), hostname(host) {
        SecInvalidateHandle(&context);
        SecInvalidateHandle(&credentials);
        try {
            if (proxy.active() && proxy.secure) {
                // Verify the proxy before releasing CONNECT credentials. This
                // outer session has no proxy, so construction cannot recurse.
                proxyTls = std::make_unique<GatewayTls>(
                    std::wstring(proxy.host.begin(), proxy.host.end()), proxy.port, trust, stopping);
            } else {
                wire = std::make_unique<GatewaySocket>(
                    proxy.active() ? std::wstring(proxy.host.begin(), proxy.host.end()) : hostname,
                    proxy.active() ? proxy.port : port, stopping);
            }
            if (proxy.active()) {
                const auto request = GatewayConnectRequest(std::string(hostname.begin(), hostname.end()), port,
                                                           proxy.authorization);
                WireSend(request.data(), request.size());
                GatewayConnectResponse response;
                while (true) {
                    if (!WireRead()) throw std::runtime_error("E_NETWORK");
                    const bool connected = response.Feed(encrypted);
                    encrypted.clear();
                    if (connected) break;
                }
            }
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
