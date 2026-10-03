#include "../../scripts/ccode/gateway-proxy.hpp"
#include <cassert>
#include <array>
#include <iostream>

int main() {
    using namespace ccode;
    ValidateGatewayEnvironmentRead(1, 0, 0); // Existing, explicitly empty NO_PROXY.
    ValidateGatewayEnvironmentRead(8, 7, 0);
    for (const auto sample : {std::array<unsigned, 3>{1, 0, 203},
                             std::array<unsigned, 3>{8, 8, 0},
                             std::array<unsigned, 3>{8, 0, 0}}) {
        bool rejected = false;
        try { ValidateGatewayEnvironmentRead(sample[0], sample[1], sample[2]); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    assert(!SelectGatewayProxy("api.example", 443, "", "", "").active());
    auto proxy = SelectGatewayProxy("api.example", 443,
        "http://user:p%40ss@proxy.example:8080/", "http://ignored.example", "");
    assert(proxy.host == "proxy.example" && proxy.port == 8080 && !proxy.secure);
    assert(proxy.authorization == "Basic dXNlcjpwQHNz");
    assert(ParseGatewayProxy("https://[::1]").port == 443);
    assert(ParseGatewayProxy("https://[::1]").host == "::1");
    assert(ParseGatewayProxy("https://[::1]").secure);
    assert(SelectGatewayProxy("api.example", 443, "", "http://fallback.example", "").host == "fallback.example");
    for (const auto* bypass : {"*", "api.example", "localhost,api.example", "localhost api.example",
                              ".example", "api.example:443", ".example:443"})
        assert(!SelectGatewayProxy("api.example", 443, "http://proxy.example", "", bypass).active());
    for (const auto* bypass : {"other.example", "api.example:80", ".notexample", "example"})
        assert(SelectGatewayProxy("api.example", 443, "http://proxy.example", "", bypass).active());
    assert(!SelectGatewayProxy("::1", 443, "http://proxy.example", "", "[::1]:443").active());
    assert(!SelectGatewayProxy("API.EXAMPLE", 443, "http://proxy.example", "", "api.example").active());
    for (const auto* uri : {"socks5://proxy", "http://", "http://proxy:0", "http://proxy:65536",
                           "http://proxy:-1", "http://proxy/path", "http://proxy?x=1", "http://proxy#x",
                           "http://proxy\r\nInjected:x", "http://u:%gg@proxy", "http://u@x@proxy",
                           "http://[::1", "http://unbracketed::1"}) {
        bool failed = false;
        try { ParseGatewayProxy(uri); }
        catch (const std::runtime_error& error) { failed = std::string(error.what()) == "E_NETWORK"; }
        assert(failed);
    }
    const auto connect = GatewayConnectRequest("api.example", 443, proxy.authorization);
    assert(connect == "CONNECT api.example:443 HTTP/1.1\r\nHost: api.example:443\r\n"
                      "Proxy-Authorization: Basic dXNlcjpwQHNz\r\n\r\n");
    assert(GatewayConnectRequest("::1", 443, "").find("CONNECT [::1]:443 ") == 0);
    const std::string accepted = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 Connection established\r\nX-Proxy: test\r\n\r\n";
    for (size_t split = 0; split < accepted.size(); ++split) {
        GatewayConnectResponse response;
        assert(!response.Feed(accepted.substr(0, split)));
        assert(response.Feed(accepted.substr(split)));
    }
    for (const auto& wire : {std::string("HTTP/1.1 407 Authentication Required\r\n\r\n"),
            std::string("HTTP/1.1 302 Redirect\r\nLocation: http://other\r\n\r\n"),
            std::string("HTTP/1.1 200 OK\r\nBad Header: x\r\n\r\n"),
            std::string("HTTP/1.1 200 OK\r\nX: a\nb\r\n\r\n"),
            std::string("HTTP/1.1 200 OK\r\n\r\ninjected"), std::string(65537, 'x')}) {
        bool failed = false;
        try { GatewayConnectResponse response; response.Feed(wire); }
        catch (const std::runtime_error& error) { failed = std::string(error.what()) == "E_NETWORK"; }
        assert(failed);
    }
    std::cout << "PASS: proxy route, credential scope and bounded CONNECT response\n";
}
