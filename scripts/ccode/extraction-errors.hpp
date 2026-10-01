#pragma once
#include <cstdint>

namespace ccode {
// GetLastError from the failed activation only. Never include paths, exception
// text or arbitrary numeric values in the public diagnostic vocabulary.
inline const char* ExtractionActivationError(std::uint32_t error) {
    switch (error) {
        case 5: return "E_EXTRACT_ACCESS";        // ERROR_ACCESS_DENIED
        case 32: return "E_EXTRACT_SHARING";     // ERROR_SHARING_VIOLATION
        case 33: return "E_EXTRACT_LOCKED";      // ERROR_LOCK_VIOLATION
        case 39:                               // ERROR_HANDLE_DISK_FULL
        case 112: return "E_EXTRACT_DISK_FULL";  // ERROR_DISK_FULL
        default: return "E_EXTRACT_ACTIVATE";
    }
}
}
