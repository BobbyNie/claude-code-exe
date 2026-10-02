#pragma once
#include <array>
#include <string>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif

namespace ccode {
// Reader returns bytes written into the supplied fixed-size buffer, zero at EOF.
// Readers must throw on short/error conditions rather than manufacture EOF.
template<class Reader> std::string StreamSha256(Reader reader) {
    std::array<unsigned char, 65536> buffer{};
    std::array<unsigned char, 32> digest{};
#ifdef _WIN32
    struct Hash {
        BCRYPT_HASH_HANDLE handle = nullptr;
        ~Hash() { if (handle) BCryptDestroyHash(handle); }
    } hash;
    if (BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &hash.handle, nullptr, 0,
                         nullptr, 0, 0) < 0) throw std::runtime_error("E_MANIFEST_HASH");
#elif defined(__APPLE__)
    CC_SHA256_CTX hash;
    if (!CC_SHA256_Init(&hash)) throw std::runtime_error("E_MANIFEST_HASH");
#else
    throw std::runtime_error("E_MANIFEST_HASH");
#endif
    for (;;) {
        const auto count = reader(buffer.data(), buffer.size());
        if (count > buffer.size()) throw std::runtime_error("E_MANIFEST_HASH");
        if (!count) break;
#ifdef _WIN32
        if (BCryptHashData(hash.handle, buffer.data(), static_cast<ULONG>(count), 0) < 0)
            throw std::runtime_error("E_MANIFEST_HASH");
#elif defined(__APPLE__)
        if (!CC_SHA256_Update(&hash, buffer.data(), static_cast<CC_LONG>(count)))
            throw std::runtime_error("E_MANIFEST_HASH");
#endif
    }
#ifdef _WIN32
    if (BCryptFinishHash(hash.handle, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
        throw std::runtime_error("E_MANIFEST_HASH");
#elif defined(__APPLE__)
    if (!CC_SHA256_Final(digest.data(), &hash)) throw std::runtime_error("E_MANIFEST_HASH");
#endif
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
}
