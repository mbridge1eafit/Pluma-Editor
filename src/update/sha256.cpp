#include "sha256.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <fstream>
#include <vector>

namespace Pluma::Update {

namespace {

class Sha256 {
public:
    Sha256() {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&m_algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
            m_algorithm = nullptr;
            return;
        }
        if (!BCRYPT_SUCCESS(BCryptCreateHash(m_algorithm, &m_hash, nullptr, 0, nullptr, 0, 0))) {
            m_hash = nullptr;
        }
    }
    ~Sha256() {
        if (m_hash) BCryptDestroyHash(m_hash);
        if (m_algorithm) BCryptCloseAlgorithmProvider(m_algorithm, 0);
    }
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;

    bool Valid() const noexcept { return m_hash != nullptr; }

    bool Update(const void* data, size_t size) {
        auto* bytes = static_cast<UCHAR*>(const_cast<void*>(data));
        while (size > 0) {
            const ULONG chunk = static_cast<ULONG>((std::min)(size, static_cast<size_t>(1u << 30)));
            if (!BCRYPT_SUCCESS(BCryptHashData(m_hash, bytes, chunk, 0))) return false;
            bytes += chunk;
            size -= chunk;
        }
        return true;
    }

    std::optional<std::string> FinishHex() {
        UCHAR digest[32]{};
        if (!BCRYPT_SUCCESS(BCryptFinishHash(m_hash, digest, sizeof(digest), 0))) return std::nullopt;
        static constexpr char kHex[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(64);
        for (UCHAR b : digest) {
            hex += kHex[b >> 4];
            hex += kHex[b & 0x0F];
        }
        return hex;
    }

private:
    BCRYPT_ALG_HANDLE m_algorithm = nullptr;
    BCRYPT_HASH_HANDLE m_hash = nullptr;
};

} // namespace

std::optional<std::string> Sha256Hex(std::string_view data) {
    Sha256 sha;
    if (!sha.Valid() || !sha.Update(data.data(), data.size())) return std::nullopt;
    return sha.FinishHex();
}

std::optional<std::string> Sha256HexOfFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    Sha256 sha;
    if (!sha.Valid()) return std::nullopt;
    std::vector<char> buffer(1 << 16);
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<size_t>(in.gcount());
        if (count > 0 && !sha.Update(buffer.data(), count)) return std::nullopt;
    }
    if (in.bad()) return std::nullopt;
    return sha.FinishHex();
}

} // namespace Pluma::Update
