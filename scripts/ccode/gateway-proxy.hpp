#pragma once
#include "gateway-http.hpp"

namespace ccode {
[[noreturn]] inline void RejectGatewayProxy() { throw std::runtime_error("E_NETWORK"); }
inline unsigned short GatewayProxyPort(const std::string& text) {
    if (text.empty()) RejectGatewayProxy();
    unsigned value = 0;
    for (unsigned char ch : text) {
        if (ch < '0' || ch > '9' || value > 6553) RejectGatewayProxy();
        value = value * 10 + ch - '0';
        if (value > 65535) RejectGatewayProxy();
    }
    if (!value) RejectGatewayProxy();
    return static_cast<unsigned short>(value);
}
inline std::string GatewayProxyDecode(const std::string& text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char ch = text[i];
        if (ch == '%') {
            if (i + 2 >= text.size()) RejectGatewayProxy();
            unsigned value = 0;
            for (size_t j = 1; j <= 2; ++j) {
                const auto digit = std::string("0123456789abcdef").find(
                    static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + j]))));
                if (digit == std::string::npos) RejectGatewayProxy();
                value = value * 16 + static_cast<unsigned>(digit);
            }
            ch = static_cast<unsigned char>(value); i += 2;
        }
        if (ch < 32 || ch == 127) RejectGatewayProxy();
        result += static_cast<char>(ch);
    }
    return result;
}
inline std::string GatewayProxyBasic(const std::string& credentials) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result = "Basic ";
    for (size_t i = 0; i < credentials.size(); i += 3) {
        const auto count = (std::min)(size_t(3), credentials.size() - i);
        unsigned value = 0;
        for (size_t j = 0; j < 3; ++j)
            value = (value << 8) | (j < count ? static_cast<unsigned char>(credentials[i + j]) : 0);
        result += alphabet[(value >> 18) & 63]; result += alphabet[(value >> 12) & 63];
        result += count > 1 ? alphabet[(value >> 6) & 63] : '=';
        result += count > 2 ? alphabet[value & 63] : '=';
    }
    return result;
}
struct GatewayProxy {
    std::string host, authorization;
    unsigned short port = 0;
    bool secure = false;
    bool active() const { return !host.empty(); }
};
inline GatewayProxy ParseGatewayProxy(const std::string& uri) {
    if (uri.size() > 8192) RejectGatewayProxy();
    for (unsigned char ch : uri) if (ch <= 32 || ch >= 127) RejectGatewayProxy();
    const auto schemeEnd = uri.find("://");
    if (schemeEnd == std::string::npos) RejectGatewayProxy();
    const auto scheme = GatewayLower(uri.substr(0, schemeEnd));
    if (scheme != "http" && scheme != "https") RejectGatewayProxy();
    GatewayProxy proxy; proxy.secure = scheme == "https"; proxy.port = proxy.secure ? 443 : 80;
    auto authority = uri.substr(schemeEnd + 3);
    if (!authority.empty() && authority.back() == '/') authority.pop_back();
    if (authority.find_first_of("/?#\\") != std::string::npos) RejectGatewayProxy();
    const auto at = authority.find('@');
    if (at != std::string::npos) {
        if (authority.find('@', at + 1) != std::string::npos) RejectGatewayProxy();
        const auto login = authority.substr(0, at);
        const auto colon = login.find(':');
        const auto user = GatewayProxyDecode(login.substr(0, colon));
        if (user.find(':') != std::string::npos) RejectGatewayProxy();
        proxy.authorization = GatewayProxyBasic(user + ":" +
            (colon == std::string::npos ? std::string() : GatewayProxyDecode(login.substr(colon + 1))));
        authority.erase(0, at + 1);
    }
    if (authority.empty()) RejectGatewayProxy();
    if (authority[0] == '[') {
        const auto close = authority.find(']');
        if (close == std::string::npos) RejectGatewayProxy();
        proxy.host = authority.substr(1, close - 1);
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') RejectGatewayProxy();
            proxy.port = GatewayProxyPort(authority.substr(close + 2));
        }
        if (proxy.host.find(':') == std::string::npos) RejectGatewayProxy();
    } else {
        const auto colon = authority.find(':');
        proxy.host = authority.substr(0, colon);
        if (colon != std::string::npos) proxy.port = GatewayProxyPort(authority.substr(colon + 1));
    }
    if (proxy.host.empty()) RejectGatewayProxy();
    for (unsigned char ch : proxy.host)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' || ch == ':' || ch == '_')) RejectGatewayProxy();
    return proxy;
}
inline bool GatewayProxyBypass(std::string host, unsigned short port, const std::string& exclusions) {
    host = GatewayLower(host);
    size_t start = 0;
    while (start < exclusions.size()) {
        start = exclusions.find_first_not_of(", \t\r\n", start);
        if (start == std::string::npos) break;
        const auto end = exclusions.find_first_of(", \t\r\n", start);
        auto rule = GatewayLower(exclusions.substr(start, end == std::string::npos ? end : end - start));
        start = end == std::string::npos ? exclusions.size() : end;
        if (rule == "*") return true;
        std::string target = rule, service;
        if (!rule.empty() && rule[0] == '[') {
            const auto close = rule.find(']');
            if (close == std::string::npos) continue;
            target = rule.substr(1, close - 1);
            if (close + 1 < rule.size()) {
                if (rule[close + 1] != ':') continue;
                service = rule.substr(close + 2);
                if (service.empty()) continue;
            }
        } else if (std::count(rule.begin(), rule.end(), ':') == 1) {
            const auto colon = rule.find(':'); target = rule.substr(0, colon); service = rule.substr(colon + 1);
            if (service.empty()) continue;
        }
        if (!service.empty()) {
            try { if (GatewayProxyPort(service) != port) continue; }
            catch (const std::runtime_error&) { continue; } // Invalid exclusion never bypasses a proxy.
        }
        if (host == target) return true;
        if (!target.empty() && target[0] == '.' &&
            (host == target.substr(1) || (host.size() > target.size() &&
                host.compare(host.size() - target.size(), target.size(), target) == 0))) return true;
    }
    return false;
}
inline GatewayProxy SelectGatewayProxy(const std::string& host, unsigned short port,
        const std::string& httpsProxy, const std::string& httpProxy, const std::string& exclusions) {
    if (GatewayProxyBypass(host, port, exclusions)) return {};
    const auto& uri = httpsProxy.empty() ? httpProxy : httpsProxy;
    return uri.empty() ? GatewayProxy{} : ParseGatewayProxy(uri);
}
inline std::string GatewayConnectRequest(const std::string& host, unsigned short port, const std::string& authorization) {
    if (host.empty() || !port || host.find_first_of(" \t\r\n/?#@\\") != std::string::npos ||
        authorization.find_first_of("\r\n") != std::string::npos) RejectGatewayProxy();
    const auto authority = (host.find(':') == std::string::npos ? host : "[" + host + "]") + ":" + std::to_string(port);
    return "CONNECT " + authority + " HTTP/1.1\r\nHost: " + authority + "\r\n" +
        (authorization.empty() ? std::string() : "Proxy-Authorization: " + authorization + "\r\n") + "\r\n";
}
class GatewayConnectResponse {
    std::string pending;
    size_t received = 0;
public:
    bool Feed(const std::string& bytes) {
        if (bytes.size() > 65536 - received) RejectGatewayProxy();
        received += bytes.size(); pending += bytes;
        while (true) {
            const auto end = pending.find("\r\n\r\n");
            if (end == std::string::npos) return false;
            const auto first = pending.find("\r\n");
            const auto status = pending.substr(0, first);
            if (status.size() < 12 || (status.substr(0, 9) != "HTTP/1.1 " && status.substr(0, 9) != "HTTP/1.0 ") ||
                status[9] < '1' || status[9] > '5' || status[10] < '0' || status[10] > '9' ||
                status[11] < '0' || status[11] > '9' || (status.size() > 12 && status[12] != ' ')) RejectGatewayProxy();
            for (unsigned char ch : status) if (ch < 32 || ch == 127) RejectGatewayProxy();
            for (size_t offset = first + 2; offset < end;) {
                const auto next = pending.find("\r\n", offset);
                const auto line = pending.substr(offset, next - offset);
                const auto colon = line.find(':');
                if (!colon || colon == std::string::npos) RejectGatewayProxy();
                for (unsigned char ch : line.substr(0, colon))
                    if (!std::isalnum(ch) && std::string("!#$%&'*+-.^_`|~").find(ch) == std::string::npos) RejectGatewayProxy();
                for (unsigned char ch : line.substr(colon + 1)) if ((ch < 32 && ch != '\t') || ch == 127) RejectGatewayProxy();
                offset = next + 2;
            }
            pending.erase(0, end + 4);
            const int code = (status[9] - '0') * 100 + (status[10] - '0') * 10 + status[11] - '0';
            if (code >= 100 && code < 200 && code != 101) continue;
            if (code < 200 || code >= 300 || !pending.empty()) RejectGatewayProxy();
            return true;
        }
    }
};
}
