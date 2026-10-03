#pragma once
#include "gateway-http.hpp"
#include <limits>

namespace ccode {
// Track authenticated HTTP message boundaries without retaining model output.
// A complete Content-Length/chunked message is independent of TLS close_notify;
// close-delimited responses require an authenticated TLS end-of-stream.
class GatewayResponseFraming {
    enum class Mode { Header, Fixed, ChunkSize, ChunkBody, ChunkEnd, Trailers, Close, Complete } mode = Mode::Header;
    std::string pending;
    size_t remaining = 0;
    bool headOnly;
    [[noreturn]] static void Reject() { throw std::runtime_error("E_NETWORK"); }
    static size_t Decimal(const std::string& text) {
        if (text.empty()) Reject();
        size_t result = 0;
        for (unsigned char ch : text) {
            if (ch < '0' || ch > '9' || result > (std::numeric_limits<size_t>::max() - (ch - '0')) / 10) Reject();
            result = result * 10 + (ch - '0');
        }
        return result;
    }
    void Header(const std::string& text) {
        const auto first = text.find("\r\n");
        const auto status = text.substr(0, first);
        if (status.size() < 12 || (status.substr(0, 9) != "HTTP/1.1 " && status.substr(0, 9) != "HTTP/1.0 ") ||
            status[9] < '1' || status[9] > '5' || status[10] < '0' || status[10] > '9' ||
            status[11] < '0' || status[11] > '9' || (status.size() > 12 && status[12] != ' ')) Reject();
        for (unsigned char ch : status) if (ch < 32 || ch == 127) Reject();
        const auto code = (status[9] - '0') * 100 + (status[10] - '0') * 10 + status[11] - '0';
        bool hasLength = false, chunked = false;
        size_t offset = first == std::string::npos ? text.size() : first + 2;
        while (offset < text.size()) {
            const auto end = text.find("\r\n", offset);
            const auto line = text.substr(offset, end == std::string::npos ? std::string::npos : end - offset);
            const auto colon = line.find(':');
            if (colon == std::string::npos || !colon) Reject();
            for (unsigned char ch : line.substr(0, colon))
                if (!std::isalnum(ch) && std::string("!#$%&'*+-.^_`|~").find(static_cast<char>(ch)) == std::string::npos) Reject();
            for (unsigned char ch : line.substr(colon + 1)) if ((ch < 32 && ch != '\t') || ch == 127) Reject();
            const auto name = GatewayLower(line.substr(0, colon));
            const auto begin = line.find_first_not_of(" \t", colon + 1);
            const auto last = line.find_last_not_of(" \t");
            const auto value = begin == std::string::npos ? std::string() : line.substr(begin, last - begin + 1);
            if (name == "content-length") {
                if (hasLength) Reject();
                hasLength = true; remaining = Decimal(value);
            } else if (name == "transfer-encoding") {
                if (chunked || GatewayLower(value) != "chunked") Reject();
                chunked = true;
            }
            offset = end == std::string::npos ? text.size() : end + 2;
        }
        if (hasLength && chunked) Reject();
        if (code < 200) { if (code == 101 || hasLength || chunked) Reject(); mode = Mode::Header; return; }
        mode = headOnly || code == 204 || code == 304 ? Mode::Complete :
            chunked ? Mode::ChunkSize : hasLength ? (remaining ? Mode::Fixed : Mode::Complete) : Mode::Close;
    }
public:
    explicit GatewayResponseFraming(bool isHead = false) : headOnly(isHead) {}
    bool Feed(const std::string& bytes) {
        if (mode == Mode::Complete) { if (!bytes.empty()) Reject(); return true; }
        pending += bytes;
        while (mode == Mode::Header) {
            const auto end = pending.find("\r\n\r\n");
            if (end == std::string::npos) { if (pending.size() > 65536) Reject(); return false; }
            if (end > 65536) Reject();
            const auto text = pending.substr(0, end); pending.erase(0, end + 4);
            Header(text);
        }
        while (mode == Mode::ChunkSize || mode == Mode::ChunkBody || mode == Mode::ChunkEnd || mode == Mode::Trailers) {
            if (mode == Mode::ChunkSize) {
                const auto end = pending.find("\r\n");
                if (end == std::string::npos) { if (pending.size() > 8192) Reject(); return false; }
                if (!end || end > 8192) Reject();
                const auto line = pending.substr(0, end);
                const auto digits = line.substr(0, line.find(';'));
                if (digits.empty()) Reject();
                for (unsigned char ch : line) if (ch < 32 || ch >= 127) Reject();
                remaining = 0;
                for (unsigned char ch : digits) {
                    const auto digit = std::string("0123456789abcdef").find(static_cast<char>(std::tolower(ch)));
                    if (digit == std::string::npos || remaining > (std::numeric_limits<size_t>::max() - digit) / 16) Reject();
                    remaining = remaining * 16 + digit;
                }
                pending.erase(0, end + 2);
                mode = remaining ? Mode::ChunkBody : Mode::Trailers;
            } else if (mode == Mode::ChunkBody) {
                const auto consumed = (std::min)(remaining, pending.size());
                remaining -= consumed; pending.erase(0, consumed);
                if (remaining) return false;
                mode = Mode::ChunkEnd;
            } else if (mode == Mode::ChunkEnd) {
                if (pending.size() < 2) return false;
                if (pending.substr(0, 2) != "\r\n") Reject();
                pending.erase(0, 2); mode = Mode::ChunkSize;
            } else {
                // Trailer support is deliberately fail-closed for now; trailers
                // must not override authentication or framing semantics.
                if (pending.size() < 2) return false;
                if (pending.substr(0, 2) != "\r\n") Reject();
                pending.erase(0, 2); mode = Mode::Complete;
            }
        }
        if (mode == Mode::Fixed) {
            if (pending.size() > remaining) Reject();
            remaining -= pending.size();
            if (!remaining) mode = Mode::Complete;
        } else if (mode == Mode::Complete && !pending.empty()) Reject();
        pending.clear();
        return mode == Mode::Complete;
    }
    void EndOfStream(bool authenticated) {
        if (mode == Mode::Complete) return;
        if (!authenticated || mode != Mode::Close) Reject();
        mode = Mode::Complete;
    }
};
}
