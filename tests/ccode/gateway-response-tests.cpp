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
    const std::string chunkHeader = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    const std::string trailerWire = chunkHeader + "3\r\nabc\r\n0\r\nX-Checksum: fixture\r\nServer-Timing: total;dur=1\r\n\r\n";
    // All transport splits, including CR/LF and trailer-name boundaries.
    for (size_t split = 0; split < trailerWire.size(); ++split) {
        ccode::GatewayResponseFraming trailers;
        assert(!trailers.Feed(trailerWire.substr(0, split)));
        assert(trailers.Feed(trailerWire.substr(split)));
        trailers.EndOfStream(false); // Complete authenticated message, no close_notify needed.
    }
    ccode::GatewayResponseFraming bytewise;
    for (size_t i = 0; i < trailerWire.size(); ++i)
        assert(bytewise.Feed(trailerWire.substr(i, 1)) == (i + 1 == trailerWire.size()));
    const auto rejects = [](const std::string& wire) {
        bool failed = false;
        try { ccode::GatewayResponseFraming parser; parser.Feed(wire); parser.EndOfStream(true); }
        catch (const std::runtime_error& error) { failed = std::string(error.what()) == "E_NETWORK"; }
        assert(failed);
    };
    for (const auto* field : {"Content-Length", "tRaNsFeR-EnCoDiNg", "Host", "Connection",
            "Authorization", "Proxy-Authorization", "WWW-Authenticate", "Set-Cookie",
            "Content-Type", "Content-Encoding", "Trailer", "Location", "Content-Range"})
        rejects(chunkHeader + "0\r\n" + field + ": forbidden\r\n\r\n");
    for (const auto* fields : {" Bad: folded\r\n", "Bad Name: value\r\n", "NoColon\r\n",
            "X-Ok: a\nb\r\n", "X-Ok: a\x7f\r\n", ": empty\r\n"})
        rejects(chunkHeader + "0\r\n" + fields + "\r\n");
    rejects("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: x-private\r\n\r\n"
            "0\r\nX-Private: forbidden\r\n\r\n");
    rejects(chunkHeader + "0\r\nX-Ok: truncated\r\n");
    rejects(trailerWire + "extra");
    rejects(chunkHeader + "0\r\nX-Large: " + std::string(8192, 'a')); // Unterminated line.
    rejects(chunkHeader + "0\r\nX-Large: " + std::string(8192, 'a') + "\r\n\r\n");
    ccode::GatewayResponseFraming maximumLine;
    assert(!maximumLine.Feed(chunkHeader + "0\r\n" + "X:" + std::string(8190, 'a') + "\r"));
    assert(maximumLine.Feed("\n\r\n"));
    ccode::GatewayResponseFraming bounded;
    assert(!bounded.Feed(chunkHeader + "0\r\n"));
    bool overflow = false;
    try {
        for (int i = 0; i < 100; ++i)
            assert(!bounded.Feed("X-Count: " + std::string(1000, 'a') + "\r\n"));
    } catch (const std::runtime_error&) { overflow = true; }
    assert(overflow); // Aggregate bound across otherwise valid small lines.
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
