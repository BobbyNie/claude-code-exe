// Build-only exporter: shares the launcher's document without running its gate
// or engine. This is not an entry point linked into the delivered launcher.
#include "boundary.hpp"
#include <iostream>
int main() {
    std::cout << ccode::BoundaryManifest().dump(2) << '\n';
    return std::cout.good() ? 0 : 1;
}
