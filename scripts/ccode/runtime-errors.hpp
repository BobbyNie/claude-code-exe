#pragma once
#include <cstdint>
namespace ccode {
// Fixed public categories only: no private pathname, raw exception or OS number.
inline const char* RuntimePayloadOpenError(std::uint32_t error) {
    switch (error) {
        case 2: return "E_RUNTIME_MISSING";
        case 3: return "E_RUNTIME_PATH";
        case 5: return "E_RUNTIME_ACCESS";
        case 32: return "E_RUNTIME_SHARING";
        default: return "E_RUNTIME_FILE";
    }
}
enum class LockedFilePurpose { Manifest, RuntimePayload };
}
