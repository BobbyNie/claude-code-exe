#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../scripts/ccode/gateway-trust.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

static std::string Read(const std::wstring& path) {
    std::ifstream input(std::filesystem::path(path), std::ios::binary);
    assert(input);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
// Match Schannel's serialized chain format: order zero identifies the peer,
// not whichever certificate happens to be enumerated first.
static std::vector<BYTE> Serialize(PCCERT_CONTEXT leaf, PCCERT_CONTEXT issuer, bool markLeaf = true) {
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
    assert(store);
    DWORD order = 0;
    for (const auto source : {leaf, issuer}) {
        PCCERT_CONTEXT added = nullptr;
        assert(CertAddCertificateContextToStore(store, source, CERT_STORE_ADD_ALWAYS, &added));
        CRYPT_DATA_BLOB property{sizeof(order), reinterpret_cast<BYTE*>(&order)};
        if (order || markLeaf)
            assert(CertSetCertificateContextProperty(added, 0xE697U, 0, &property));
        CertFreeCertificateContext(added);
        ++order;
    }
    CRYPT_DATA_BLOB bytes{};
    assert(CertSaveStore(store, X509_ASN_ENCODING, CERT_STORE_SAVE_AS_STORE,
                         CERT_STORE_SAVE_TO_MEMORY, &bytes, 0));
    std::vector<BYTE> result(bytes.cbData);
    bytes.pbData = result.data();
    assert(CertSaveStore(store, X509_ASN_ENCODING, CERT_STORE_SAVE_AS_STORE,
                         CERT_STORE_SAVE_TO_MEMORY, &bytes, 0));
    assert(CertCloseStore(store, CERT_CLOSE_STORE_CHECK_FLAG));
    return result;
}
int wmain(int argc, wchar_t** argv) {
    assert(argc == 2);
    const std::wstring folder = argv[1];
    const auto goodPem = Read(folder + L"/untrusted-test-cert.pem");
    const auto expiredPem = Read(folder + L"/expired-test-cert.pem");
    const auto validPem = Read(folder + L"/valid-test-cert.pem");
    const auto caPem = Read(folder + L"/expired-test-ca.pem");
    ccode::GatewayTrust defaults;
    const auto good = ccode::GatewayTrust::Decode(goodPem);
    const auto expired = ccode::GatewayTrust::Decode(expiredPem);
    assert(defaults.Verify(good.get(), L"127.0.0.1") == ccode::GatewayCertificateResult::Rejected);
    {
        ccode::GatewayTrust scoped(goodPem);
        assert(scoped.Verify(good.get(), L"127.0.0.1") == ccode::GatewayCertificateResult::Trusted);
        assert(scoped.Verify(good.get(), L"wrong.invalid") == ccode::GatewayCertificateResult::Rejected);
    }
    // Trust additions must not mutate the user's or machine's root store.
    assert(defaults.Verify(good.get(), L"127.0.0.1") == ccode::GatewayCertificateResult::Rejected);
    ccode::GatewayTrust issuer(caPem);
    const auto valid = ccode::GatewayTrust::Decode(validPem);
    const auto ca = ccode::GatewayTrust::Decode(caPem);
    assert(!CertComparePublicKeyInfo(X509_ASN_ENCODING, &ca->pCertInfo->SubjectPublicKeyInfo,
                                    &expired->pCertInfo->SubjectPublicKeyInfo));
    assert(issuer.Verify(valid.get(), L"127.0.0.1") == ccode::GatewayCertificateResult::Trusted);
    const auto expiry = issuer.Inspect(expired.get(), L"127.0.0.1");
    if (expiry.result != ccode::GatewayCertificateResult::Expired)
        std::cerr << "E_TEST_CERTIFICATE_POLICY policy=" << std::hex << expiry.policyError
                  << " chain=" << expiry.chainErrors << std::dec << std::endl;
    assert(expiry.result == ccode::GatewayCertificateResult::Expired);
    for (const auto peer : {valid.get(), expired.get()}) {
        auto serialized = Serialize(peer, ca.get());
        CRYPT_DATA_BLOB blob{static_cast<DWORD>(serialized.size()), serialized.data()};
        const auto restored = ccode::GatewayTrust::DeserializePeer(blob);
        assert(CertCompareCertificate(X509_ASN_ENCODING, peer->pCertInfo, restored->pCertInfo));
        assert(issuer.Verify(restored.get(), L"127.0.0.1") == issuer.Verify(peer, L"127.0.0.1"));
        // A peer that is both expired and wrong-host may report expiry first.
        // Serialization must preserve the original decision and never trust it.
        const auto wrongHost = issuer.Verify(restored.get(), L"wrong.invalid");
        assert(wrongHost == issuer.Verify(peer, L"wrong.invalid"));
        assert(wrongHost != ccode::GatewayCertificateResult::Trusted);
    }
    auto unmarked = Serialize(valid.get(), ca.get(), false);
    for (auto bytes : {unmarked, std::vector<BYTE>{1, 2, 3}}) {
        bool rejected = false;
        try { ccode::GatewayTrust::DeserializePeer({static_cast<DWORD>(bytes.size()), bytes.data()}); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    for (const auto& invalid : {std::string("not a certificate"), goodPem + "trailing garbage"}) {
        bool rejected = false;
        try { ccode::GatewayTrust malformed(invalid); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    std::cout << "PASS: native scoped CA trust, untrusted/hostname/expiry rejection, no root-store mutation\n";
    return 0;
}
