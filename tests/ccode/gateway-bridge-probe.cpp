#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../scripts/ccode/gateway-bridge.hpp"
#include <iostream>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 && argc != 3) return 2;
    try {
        ccode::GatewayBridge bridge(argv[1], argc == 3 ? argv[2] : L"");
        // Test executable only: report capability to the driving fixture.
        const auto url = bridge.Url();
        std::cout << std::string(url.begin(), url.end()) << std::endl;
        std::string line; std::getline(std::cin, line);
        bridge.Stop();
        std::cout << bridge.Error() << std::endl;
        return 0;
    } catch (...) { std::cerr << "E_TEST_GATEWAY" << std::endl; return 1; }
}
