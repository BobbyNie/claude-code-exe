#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ccode {
enum class GatewayCertificateResult { Trusted, Expired, Rejected };

// Per-invocation certificate trust. Never imports roots into a persistent store.
// This policy must be applied to the peer of the actual TLS connection before
// releasing the final handshake token or any HTTP credentials/body.
class GatewayTrust {
    struct CertificateDeleter {
        void operator()(const CERT_CONTEXT* value) const { if (value) CertFreeCertificateContext(value); }
    };
    struct Store {
        HCERTSTORE value;
        explicit Store(HCERTSTORE handle) : value(handle) { if (!value) throw std::runtime_error("E_GATEWAY_TLS"); }
        ~Store() { CertCloseStore(value, 0); }
        Store(const Store&) = delete;
        Store& operator=(const Store&) = delete;
    };
    Store roots{CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr)};
    HCERTCHAINENGINE engine = nullptr;
    static void Add(HCERTSTORE store, PCCERT_CONTEXT certificate) {
        if (!CertAddCertificateContextToStore(store, certificate, CERT_STORE_ADD_USE_EXISTING, nullptr))
            throw std::runtime_error("E_GATEWAY_TLS");
    }
public:
    using Certificate = std::unique_ptr<const CERT_CONTEXT, CertificateDeleter>;
    // Microsoft MsQuic cert_capi.c documents Schannel's chain-order property.
    // Keep the serialized store alive through the returned leaf context so
    // intermediate certificates remain available to the chain engine.
    static Certificate DeserializePeer(const CRYPT_DATA_BLOB& blob) {
        if (!blob.pbData || !blob.cbData || blob.cbData > 4 * 1024 * 1024)
            throw std::runtime_error("E_GATEWAY_TLS");
        Store store(CertOpenStore(CERT_STORE_PROV_SERIALIZED, X509_ASN_ENCODING, 0,
            CERT_STORE_DEFER_CLOSE_UNTIL_LAST_FREE_FLAG, &blob));
        Certificate leaf;
        PCCERT_CONTEXT item = nullptr;
        while ((item = CertEnumCertificatesInStore(store.value, item)) != nullptr) {
            DWORD order = 0, length = sizeof(order);
            if (!CertGetCertificateContextProperty(item, 0xE697U, &order, &length) || length != sizeof(order)) {
                CertFreeCertificateContext(item);
                throw std::runtime_error("E_GATEWAY_TLS");
            }
            if (!order) {
                if (leaf) {
                    CertFreeCertificateContext(item);
                    throw std::runtime_error("E_GATEWAY_TLS");
                }
                leaf.reset(CertDuplicateCertificateContext(item));
            }
        }
        if (!leaf) throw std::runtime_error("E_GATEWAY_TLS");
        return leaf;
    }
    static Certificate Decode(const std::string& pem) {
        if (pem.size() > 4 * 1024 * 1024) throw std::runtime_error("E_GATEWAY_TLS");
        DWORD length = 0;
        if (!CryptStringToBinaryA(pem.c_str(), static_cast<DWORD>(pem.size()), CRYPT_STRING_BASE64HEADER,
                nullptr, &length, nullptr, nullptr)) throw std::runtime_error("E_GATEWAY_TLS");
        std::vector<BYTE> bytes(length);
        if (!CryptStringToBinaryA(pem.c_str(), static_cast<DWORD>(pem.size()), CRYPT_STRING_BASE64HEADER,
                bytes.data(), &length, nullptr, nullptr)) throw std::runtime_error("E_GATEWAY_TLS");
        Certificate result(CertCreateCertificateContext(X509_ASN_ENCODING, bytes.data(), length));
        if (!result) throw std::runtime_error("E_GATEWAY_TLS");
        return result;
    }
    explicit GatewayTrust(const std::string& extraPem = {}) {
        // ROOT's logical current-user view includes the applicable machine roots.
        Store system(CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
            CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG, L"ROOT"));
        PCCERT_CONTEXT item = nullptr;
        while ((item = CertEnumCertificatesInStore(system.value, item)) != nullptr) {
            if (!CertAddCertificateContextToStore(roots.value, item, CERT_STORE_ADD_USE_EXISTING, nullptr)) {
                CertFreeCertificateContext(item);
                throw std::runtime_error("E_GATEWAY_TLS");
            }
        }
        if (extraPem.size() > 4 * 1024 * 1024) throw std::runtime_error("E_GATEWAY_TLS");
        size_t offset = 0;
        const std::string begin = "-----BEGIN CERTIFICATE-----", end = "-----END CERTIFICATE-----";
        while (offset < extraPem.size()) {
            offset = extraPem.find_first_not_of(" \r\n\t", offset);
            if (offset == std::string::npos) break;
            if (extraPem.compare(offset, begin.size(), begin) != 0) throw std::runtime_error("E_GATEWAY_TLS");
            const auto finish = extraPem.find(end, offset + begin.size());
            if (finish == std::string::npos) throw std::runtime_error("E_GATEWAY_TLS");
            const auto certificate = Decode(extraPem.substr(offset, finish + end.size() - offset));
            Add(roots.value, certificate.get());
            offset = finish + end.size();
        }
        CERT_CHAIN_ENGINE_CONFIG config{};
        config.cbSize = sizeof(config);
        config.hExclusiveRoot = roots.value;
        // Exclusive roots define trust, but the issuer search also needs these
        // certificates in the chain engine world store (not only root policy).
        config.cAdditionalStore = 1;
        config.rghAdditionalStore = &roots.value;
        // Preserve OS chain policy (including disallowed certificates); only the
        // trusted-root set is invocation-local. Never set ignore-error flags.
        if (!CertCreateCertificateChainEngine(&config, &engine)) throw std::runtime_error("E_GATEWAY_TLS");
    }
    ~GatewayTrust() { if (engine) CertFreeCertificateChainEngine(engine); }
    GatewayTrust(const GatewayTrust&) = delete;
    GatewayTrust& operator=(const GatewayTrust&) = delete;
    struct Verification {
        GatewayCertificateResult result = GatewayCertificateResult::Rejected;
        DWORD policyError = 0, chainErrors = 0;
    };
    Verification Inspect(PCCERT_CONTEXT peer, const std::wstring& hostname) const {
        if (!peer || hostname.empty() || hostname.find(L'\0') != std::wstring::npos)
            return {};
        CERT_CHAIN_PARA parameters{}; parameters.cbSize = sizeof(parameters);
        LPSTR serverAuth = const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH);
        parameters.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
        parameters.RequestedUsage.Usage.cUsageIdentifier = 1;
        parameters.RequestedUsage.Usage.rgpszUsageIdentifier = &serverAuth;
        PCCERT_CHAIN_CONTEXT chain = nullptr;
        if (!CertGetCertificateChain(engine, peer, nullptr, peer->hCertStore, &parameters, 0, nullptr, &chain))
            return {};
        SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl{}; ssl.cbSize = sizeof(ssl);
        ssl.dwAuthType = AUTHTYPE_SERVER; ssl.pwszServerName = const_cast<LPWSTR>(hostname.c_str());
        CERT_CHAIN_POLICY_PARA policy{}; policy.cbSize = sizeof(policy); policy.pvExtraPolicyPara = &ssl;
        CERT_CHAIN_POLICY_STATUS status{}; status.cbSize = sizeof(status);
        const BOOL valid = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain, &policy, &status);
        const DWORD errors = chain->TrustStatus.dwErrorStatus;
        CertFreeCertificateChain(chain);
        const auto result = !valid ? GatewayCertificateResult::Rejected :
            !status.dwError ? GatewayCertificateResult::Trusted :
            status.dwError == static_cast<DWORD>(CERT_E_EXPIRED) ? GatewayCertificateResult::Expired : GatewayCertificateResult::Rejected;
        return {result, status.dwError, errors};
    }
    GatewayCertificateResult Verify(PCCERT_CONTEXT peer, const std::wstring& hostname) const {
        return Inspect(peer, hostname).result;
    }
};
} // namespace ccode
