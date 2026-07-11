#include "../../scripts/ccode/common.hpp"

#include <cassert>
#include <iostream>

int main() {
    using namespace ccode;

    assert(MapEnvironmentName(L"ANTHROPIC_DEFAULT_HAIKU_MODEL") == L"A_DEFAULT_HAIKU_MODEL");
    assert(MapEnvironmentName(L"anthropic_api_key") == L"A_api_key");
    assert(MapEnvironmentName(L"CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC") ==
           L"C_DISABLE_NONESSENTIAL_TRAFFIC");
    assert(MapEnvironmentName(L"PATH") == L"PATH");

    assert(RewritePath(L"data\\Claude\\settings.json") == L"data\\cc\\settings.json");
    assert(RewritePath(L"DATA\\ANTHROPIC\\Claude") == L"DATA\\aa\\cc");
    assert(!ContainsForbiddenText(RewritePath(L"data\\ANTHROPIC\\CLAUDE")));

    assert(IsValidGatewayUrl(L"https://gateway.example.test"));
    assert(IsValidGatewayUrl(L"https://api.deepseek.com/anthropic"));
    assert(IsValidGatewayUrl(L"https://api.anthropic.example"));
    assert(IsValidGatewayUrl(L"http://gateway.example.test"));
    assert(!IsValidGatewayUrl(L""));
    assert(GatewayHost(L"https://gateway.example.test/v1") == L"gateway.example.test");
    assert(GatewayHost(L"http://gateway.example.test/v1") == L"gateway.example.test");
    assert(IsAllowedNetworkHost(L"gateway.example.test", L"https://gateway.example.test/v1"));
    assert(!IsAllowedNetworkHost(L"example.org", L"https://gateway.example.test/v1"));

    std::cout << "ccode native isolation tests passed\n";
    return 0;
}
