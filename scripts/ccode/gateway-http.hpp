#pragma once
#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ccode {
struct GatewayRequest {
    std::string method, target, body;
    std::vector<std::pair<std::string, std::string>> headers;
};
inline std::string GatewayLower(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
inline std::string GatewayTrim(const std::string& text) {
    const auto begin = text.find_first_not_of(" \t");
    if (begin == std::string::npos) return {};
    return text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
}
// One request per loopback connection. No pipelining or ambiguous framing.
class GatewayRequestParser {
    static constexpr size_t HeaderLimit = 65536, BodyLimit = 64 * 1024 * 1024;
    std::string prefix, pending;
    GatewayRequest request;
    size_t length = 0;
    bool headersRead = false, complete = false;
    [[noreturn]] static void Reject() { throw std::runtime_error("E_GATEWAY"); }
    void Headers(const std::string& raw) {
        const auto lineEnd = raw.find("\r\n");
        const auto line = raw.substr(0, lineEnd);
        const auto first = line.find(' '), last = line.rfind(' ');
        if (first == std::string::npos || first == last || line.substr(last + 1) != "HTTP/1.1") Reject();
        request.method = line.substr(0, first);
        if (request.method != "POST" && request.method != "GET" && request.method != "DELETE" &&
            request.method != "PUT" && request.method != "PATCH" && request.method != "HEAD" &&
            request.method != "OPTIONS") Reject();
        const auto target = line.substr(first + 1, last - first - 1);
        if (target.rfind(prefix + "/", 0) != 0) Reject();
        request.target = target.substr(prefix.size());
        for (unsigned char ch : request.target)
            if (ch <= 32 || ch >= 127 || ch == '#' || ch == '\\') Reject();
        bool hasLength = false;
        std::vector<std::string> connectionFields;
        for (size_t pos = lineEnd == std::string::npos ? raw.size() : lineEnd + 2; pos < raw.size();) {
            const auto end = raw.find("\r\n", pos);
            const auto item = raw.substr(pos, end == std::string::npos ? end : end - pos);
            pos = end == std::string::npos ? raw.size() : end + 2;
            const auto colon = item.find(':');
            if (colon == std::string::npos || !colon) Reject();
            const auto name = GatewayLower(item.substr(0, colon));
            const auto value = GatewayTrim(item.substr(colon + 1));
            for (unsigned char ch : name)
                if (!(std::isalnum(ch) || std::string("!#$%&'*+-.^_`|~").find(ch) != std::string::npos)) Reject();
            for (unsigned char ch : value) if ((ch < 32 && ch != '\t') || ch == 127) Reject();
            if (name == "connection") {
                size_t start = 0;
                do {
                    const auto comma = value.find(',', start);
                    connectionFields.push_back(GatewayLower(GatewayTrim(value.substr(start,
                        comma == std::string::npos ? comma : comma - start))));
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                } while (start < value.size());
            }
            if (name == "content-length") {
                if (hasLength || value.empty() || value.find_first_not_of("0123456789") != std::string::npos) Reject();
                hasLength = true;
                for (char c : value) {
                    if (length > BodyLimit / 10) Reject();
                    length = length * 10 + (c - '0');
                    if (length > BodyLimit) Reject();
                }
            } else if (name == "transfer-encoding" || name == "expect") Reject();
            else if (name != "host" && name != "connection" && name != "accept-encoding" &&
                     name != "proxy-authorization" && name != "proxy-connection" &&
                     name != "keep-alive" && name != "te" && name != "trailer" && name != "upgrade")
                request.headers.emplace_back(name, value);
        }
        request.headers.erase(std::remove_if(request.headers.begin(), request.headers.end(),
            [&](const auto& header) {
                return std::find(connectionFields.begin(), connectionFields.end(), header.first) != connectionFields.end();
            }), request.headers.end());
    }
public:
    explicit GatewayRequestParser(std::string capabilityPrefix) : prefix(std::move(capabilityPrefix)) {}
    bool Feed(const std::string& data) {
        if (complete) Reject();
        if (pending.size() + data.size() > BodyLimit + HeaderLimit) Reject();
        pending += data;
        if (!headersRead) {
            const auto end = pending.find("\r\n\r\n");
            if (end == std::string::npos) { if (pending.size() > HeaderLimit) Reject(); return false; }
            if (end > HeaderLimit) Reject();
            Headers(pending.substr(0, end));
            pending.erase(0, end + 4);
            headersRead = true;
        }
        if (pending.size() > length) Reject();
        if (pending.size() < length) return false;
        request.body = std::move(pending);
        return complete = true;
    }
    const GatewayRequest& Request() const {
        if (!complete) Reject();
        return request;
    }
};
}
