#include "../../scripts/ccode/gateway-http.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace ccode;
    GatewayRequestParser parser("/capability");
    assert(!parser.Feed("POST /capability/v1/messages?beta=true HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 7\r\nAuthorization: Bearer test-secret\r\nContent-Type: application/json\r\n\r\n{\"a"));
    assert(parser.Feed("\":1}"));
    const auto request = parser.Request();
    assert(request.method == "POST");
    assert(request.target == "/v1/messages?beta=true");
    assert(request.body == "{\"a\":1}");
    assert(request.headers.size() == 2);
    assert(request.headers[0].first == "authorization");
    assert(request.headers[0].second == "Bearer test-secret");
    for (const auto& wire : {
        "GET /capability/x#fragment HTTP/1.1\r\nHost: x\r\n\r\n",
        "GET /capability/x HTTP/1.1\r\nBad Header: x\r\n\r\n",
        "GET /capability/x HTTP/1.1\r\nX-Test: x\ny\r\n\r\n",
        "GET /wrong/x HTTP/1.1\r\nHost: x\r\n\r\n",
        "POST /capability/x HTTP/1.1\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",
        "POST /capability/x HTTP/1.1\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n"
    }) {
        bool rejected = false;
        try { GatewayRequestParser invalid("/capability"); invalid.Feed(wire); }
        catch (const std::runtime_error& error) { rejected = std::string(error.what()) == "E_GATEWAY"; }
        assert(rejected);
    }
    GatewayRequestParser hop("/capability");
    assert(hop.Feed("GET /capability/x HTTP/1.1\r\nX-Private-Hop: never-upstream\r\nConnection: X-Private-Hop\r\n\r\n"));
    assert(hop.Request().headers.empty());
    std::cout << "Gateway HTTP request contracts passed\n";
}
