#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>

namespace ccode {
// A bounded header/machine gate, not a replacement for the OS PE loader.
// Reader must read from the retained authenticated executable handle.
template<class Reader> void RequireAmd64PeImage(uint64_t fileSize, Reader read) {
    const auto reject = [] { throw std::runtime_error("E_MANIFEST_PE"); };
    auto bytes = [&](uint64_t offset, size_t count) {
        if (offset > fileSize || count > fileSize - offset) reject();
        const auto result = read(offset, count);
        if (result.size() != count) reject();
        return result;
    };
    const auto u16 = [](const std::string& value, size_t offset) {
        return uint16_t(static_cast<unsigned char>(value[offset])) |
            (uint16_t(static_cast<unsigned char>(value[offset + 1])) << 8);
    };
    const auto u32 = [](const std::string& value, size_t offset) {
        uint32_t result = 0;
        for (size_t i = 0; i < 4; ++i)
            result |= uint32_t(static_cast<unsigned char>(value[offset + i])) << (8 * i);
        return result;
    };
    const auto dos = bytes(0, 64);
    const uint64_t peOffset = u32(dos, 60);
    if (u16(dos, 0) != 0x5a4d || peOffset < 64 || peOffset > 0x7fffffff) reject();
    const auto coff = bytes(peOffset, 24);
    const auto sections = u16(coff, 6), optionalSize = u16(coff, 20);
    const auto characteristics = u16(coff, 22);
    if (u32(coff, 0) != 0x4550 || u16(coff, 4) != 0x8664 ||
        !sections || sections > 96 || optionalSize < 112 || optionalSize > 4096 ||
        !(characteristics & 2) || (characteristics & 0x2000)) reject();
    const uint64_t optionalOffset = peOffset + 24;
    const uint64_t headerSpan = uint64_t(optionalSize) + uint64_t(sections) * 40;
    if (optionalOffset > fileSize || headerSpan > fileSize - optionalOffset ||
        u16(bytes(optionalOffset, 2), 0) != 0x20b) reject();
}
}
