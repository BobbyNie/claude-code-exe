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
int wmain(int argc, wchar_t** argv) {
    assert(argc == 2);
    const std::wstring folder = argv[1];
    const auto goodPem = Read(folder + L"/untrusted-test-cert.pem");
    const auto expiredPem = Read(folder + L"/expired-test-cert.pem");
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
    assert(issuer.Verify(expired.get(), L"127.0.0.1") == ccode::GatewayCertificateResult::Expired);
    for (const auto& invalid : {std::string("not a certificate"), goodPem + "trailing garbage"}) {
        bool rejected = false;
        try { ccode::GatewayTrust malformed(invalid); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    std::cout << "PASS: native scoped CA trust, untrusted/hostname/expiry rejection, no root-store mutation\n";
    return 0;
}
