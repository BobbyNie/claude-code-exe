#include "../../scripts/ccode/gateway-response.hpp"
#include <cassert>
#include <iostream>
int main() {
    ccode::GatewayResponseFraming length;
    assert(!length.Feed("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na"));
    assert(length.Feed("bc"));
    ccode::GatewayResponseFraming noBody;
    assert(noBody.Feed("HTTP/1.1 204 No Content\r\n\r\n"));
    bool rejected = false;
    try {
        ccode::GatewayResponseFraming truncated;
        truncated.Feed("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na");
        truncated.EndOfStream(true);
    } catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    ccode::GatewayResponseFraming chunks;
    assert(!chunks.Feed("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nab"));
    assert(!chunks.Feed("c\r\n0\r\n"));
    assert(chunks.Feed("\r\n"));
    for (const auto& wire : {
        "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nx!!",
        "HTTP/1.1 200 OK\r\nBad Header: value\r\nContent-Length: 0\r\n\r\n",
        "HTTP/1.1 200 OK\r\nX-Test: x\ny\r\nContent-Length: 0\r\n\r\n"
    }) {
        bool failed = false;
        try { ccode::GatewayResponseFraming invalid; invalid.Feed(wire); }
        catch (const std::runtime_error&) { failed = true; }
        assert(failed);
    }
    ccode::GatewayResponseFraming closed;
    assert(!closed.Feed("HTTP/1.1 200 OK\r\n\r\nbody"));
    bool failedClose = false;
    try { closed.EndOfStream(false); }
    catch (const std::runtime_error&) { failedClose = true; }
    assert(failedClose);
    closed.EndOfStream(true);
    assert(closed.Feed(""));
    std::cout << "PASS: authenticated response framing boundaries\n";
}
